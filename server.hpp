#include <string.h>
#include <string>
#include <map>
using namespace std;

typedef enum{
    HTTP_HEADER,
    BAD_REQUEST,
    NOT_FOUND

}messageType;

string Messages[] = {
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