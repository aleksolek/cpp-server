#include "response.hpp"
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

std::string Messages[] = {
    "HTTP/1.1 200 OK\r\n",
    "HTTP/1.0 400 Bad request\r\n Content-Type: text/html\r\n\r\n <!doctype html><html><body>System is busy right now.</body></html>",
    "HTTP/1.0 404 File not found\r\n Content-Type: text/html\r\n\r\n <!doctype html><html><body>Sorry but 404NotFound :(</body></html>"
};

std::map<std::string, std::string> mimeTypes = {
    {"aac",  "audio/aac"},
    {"avi",  "video/x-msvideo"},
    {"bmp",  "image/bmp"},
    {"css",  "text/css"},
    {"gif",  "image/gif"},
    {"ico",  "image/vnd.microsoft.icon"},
    {"js",   "text/javascript"},
    {"json", "application/json"},
    {"mp3",  "audio/mpeg"},
    {"mp4",  "video/mp4"},
    {"otf",  "font/otf"},
    {"png",  "image/png"},
    {"php",  "application/x-httpd-php"},
    {"rtf",  "application/rtf"},
    {"svg",  "image/svg+xml"},
    {"txt",  "text/plain"},
    {"webm", "video/webm"},
    {"webp", "image/webp"},
    {"woff", "font/woff"},
    {"zip",  "application/zip"},
    {"html", "text/html"},
    {"htm",  "text/html"},
    {"jpeg", "image/jpeg"},
    {"jpg",  "image/jpeg"}
};

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