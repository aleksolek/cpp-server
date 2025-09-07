#pragma once

#include <string>
#include <map>

typedef enum{
    HTTP_HEADER,
    BAD_REQUEST,
    NOT_FOUND
}messageType;



std::string getContentType(const std::string &fileExtension);
void send_message(int fd, std::string filePath, std::string contentType);