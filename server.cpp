#include "server.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <sstream>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

constexpr int client_message_SIZE = 1024;
constexpr int PORT = 8080;

struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

// Get rid of .. and double slashes // in the path
std::string normalizePath(const std::string &path) {
  std::vector<std::string> parts;
  std::istringstream ss(path);
  std::string token;
  while (std::getline(ss, token, '/')) {
    if (token == "" || token == ".")
      continue;
    if (token == ".." && !parts.empty())
      parts.pop_back();
    else if (token != "..")
      parts.push_back(token);
  }
  std::string clean;
  for (const auto &p : parts)
    clean += "/" + p;
  return clean.empty() ? "/" : clean;
}

HttpRequest parseHttpRequest(const std::string &receivedMessage) {
  HttpRequest parsedRequest;
  // First line is: METHOD PATH VERSION
  // For example GET /abc.html HTTP/1.1
  std::istringstream requestStream(receivedMessage);
  std::string line;
  if (std::getline(requestStream, line)) {
    std::istringstream firstLineStream(line);
    firstLineStream >> parsedRequest.method >> parsedRequest.path >>
        parsedRequest.version;
  }

  // Normalize path to prevent directory traversal
  parsedRequest.path = normalizePath(parsedRequest.path);
  // Default to index.html if root requested
  if (parsedRequest.path == "/") {
    parsedRequest.path = "/index.html";
  }

  while ((getline(requestStream, line)) && !line.empty() && (line != "\r")) {
    auto colonPos = line.find(":");
    if (colonPos != std::string::npos) {
      std::string key = line.substr(0, colonPos);
      std::string value = line.substr(colonPos + 1);

      // Trim spaces
      key.erase(key.find_last_not_of(" \t\r\n") + 1);
      value.erase(0, value.find_first_not_of(" \t\r\n"));
      parsedRequest.headers[key] = value;
    }
  }

  if (parsedRequest.method == "POST") {
    auto it = parsedRequest.headers.find("Content-Length");
    if (it != parsedRequest.headers.end()) {
      size_t length = std::stoul(it->second);
      parsedRequest.body.resize(length);
      requestStream.read(&parsedRequest.body[0], length);
    }
  }
  return parsedRequest;
}

std::queue<int> socketQueue;
std::mutex queueMutex;
std::mutex dataMutex;
std::condition_variable queueCV;
std::atomic<bool> serverRunning(true);

void send_message(int fd, std::string filePath, std::string contentType) {
  std::string basePath = "./public";
  if (filePath.empty()) {
    filePath = basePath + "/";
  } else if (filePath.front() != '/') {
    filePath = basePath + "/" + filePath;
  } else {
    filePath = basePath + filePath;
  }
  struct stat stat_buf;

  int fdimg = open(filePath.c_str(), O_RDONLY);

  if (fdimg < 0) {
    perror(("Cannot open file: " + filePath).c_str());
    write(fd, Messages[NOT_FOUND].c_str(), Messages[NOT_FOUND].length());
    return;
  }

  fstat(fdimg, &stat_buf);
  off_t fileSize = stat_buf.st_size;

  // Build HTTP header
  std::ostringstream header;
  header << "HTTP/1.1 200 OK\r\n";
  header << contentType;
  header << "Content-Length: " << fileSize << "\r\n";
  header << "Connection: close\r\n\r\n";

  std::string headerStr = header.str();
  write(fd, headerStr.c_str(), headerStr.size());

  off_t offset = 0;
  ssize_t sent_bytes;
  while (offset < fileSize) {
    sent_bytes = sendfile(fd, fdimg, &offset, fileSize - offset);
    if (sent_bytes <= 0)
      break;
  }

  close(fdimg);
}

std::string getContentType(const std::string &fileExtension) {
  auto it = mimeTypes.find(fileExtension);
  if (it != mimeTypes.end()) {
    return "Content-Type: " + it->second + "\r\n\r\n";
  } else {
    std::cout << "Unknown file extension: " << fileExtension
              << ", defaulting to text/html\n";
    return "Content-Type: text/html\r\n\r\n";
  }
}

void connection_handler(int sock) {
  std::vector<char> buffer(client_message_SIZE);
  int bytesRead = read(sock, buffer.data(), buffer.size());
  if (bytesRead <= 0) {
    close(sock);
    return;
  }
  std::string rawRequest(buffer.data(), bytesRead);

  // Parse HTTP request
  HttpRequest request = parseHttpRequest(rawRequest);

  // Extract file extension
  std::string::size_type dotPos = request.path.find_last_of('.');
  std::string fileExtension =
      (dotPos != std::string::npos) ? request.path.substr(dotPos + 1) : "html";

  if (request.method == "POST") {
    if (!request.body.empty()) {
      // For now just log it:
      std::cout << request.body << std::endl;
    }
  }

  // Build and send response
  send_message(sock, request.path, getContentType(fileExtension));

  std::cout << std::endl << "------ exiting connection -------" << std::endl;
  close(sock);

  return;
}
void workerThread() {
  while (serverRunning) {
    int clientSocket;

    // Lock queue
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      queueCV.wait(lock, [] { return !socketQueue.empty() || !serverRunning; });

      if (!serverRunning && socketQueue.empty())
        break;

      clientSocket = socketQueue.front();
      socketQueue.pop();
    }

    // Handler connection
    connection_handler(clientSocket);
  }

  // Clean up all remaining sockets
  while (!socketQueue.empty()) {
    int s = socketQueue.front();
    socketQueue.pop();
    close(s);
  }
}

void signalHandler(int signum) {
  std::cout << "\nInterrupt signal (" << signum
            << ") received. Shutting down...\n";
  serverRunning = false;
  queueCV.notify_all();
}

int main(int argc, char const *argv[]) {
  signal(SIGINT, signalHandler);
  const int THREAD_POOL_SIZE = 4;
  std::vector<std::thread> workers;
  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    workers.emplace_back(workerThread);
  }

  /* Create TCP socket */
  int server_socket;
  int client_socket;

  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  /* Check if socket was created */
  if (server_socket < 0) {
    perror("socket could not be created");
    exit(EXIT_FAILURE);
  }

  /* Server listens on PORT from any IP address */
  struct sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(PORT);

  /* Bind the socket */
  if (bind(server_socket, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    perror("socket could not be bind");
    exit(EXIT_FAILURE);
  }
  if (listen(server_socket, 10) < 0) {
    perror("Error in listening");
    exit(EXIT_FAILURE);
  }

  /* Listen for connection from client */
  struct sockaddr_in client_address;
  char ip4[INET_ADDRSTRLEN];
  socklen_t len = sizeof(client_address);
  std::cout << "Listening on port: " << PORT << std::endl;
  while (serverRunning) {
    client_socket =
        accept(server_socket, (struct sockaddr *)&client_address, &len);
    if (client_socket < 0) {
      if (errno == EINTR)
        continue; // interrupted by a signal. Check serverRunning
      perror("Unable to accept connection");
      break;
    } else {
      inet_ntop(AF_INET, &(client_address.sin_addr), ip4, INET_ADDRSTRLEN);
      std::cout << "Client " << ip4 << " connected";
    }
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      socketQueue.push(client_socket);
      queueCV.notify_one();
    }
  }

  for (auto &t : workers) {
    if (t.joinable())
      t.join();
  }
  close(server_socket);
  std::cout << "Server closed cleanly" << std::endl;
}