#include "http.hpp"
#include <sstream>
#include <vector>

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