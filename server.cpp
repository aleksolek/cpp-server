#include "server.hpp"
#include "http.hpp"
#include "response.hpp"



// #include <csignal>
// #include <cstdlib>
// #include <cstring>
// #include <fcntl.h>
#include <iostream>
// #include <mutex>

#include <queue>
// #include <sstream>
// #include <stdio.h>
// #include <sys/sendfile.h>
// #include <sys/stat.h>
// #include <thread>
#include <unistd.h>
// #include <unordered_map>
// #include <vector>

constexpr int client_message_SIZE = 4096;

std::atomic<bool> serverRunning(true);
std::condition_variable queueCV;
std::mutex queueMutex;
std::queue<int> socketQueue;



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
void workerThread(void) {
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