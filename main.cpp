#include "server.hpp"
#include "http.hpp"
#include "response.hpp"
#include <thread>
#include <vector>
#include <csignal>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>

constexpr int PORT = 8080;

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