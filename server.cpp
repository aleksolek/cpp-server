#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include "server.h"

#define client_message_SIZE 1024
#define PORT 8080

sem_t mutex;
int thread_count = 0;
std::vector<std::string> serverData;

string getStr(string sql, char end){
    int counter = 0;
    string retStr = "";
    while(sql[counter] != '\0'){
        if(sql[counter] == end){
            break;
        }
        retStr += sql[counter];
        counter++;
    }
    return retStr;
}

void send_message(int fd, string filePath, string headerFile){
    string header = Messages[HTTP_HEADER] + headerFile;
    filePath = "/public" + filePath;
    struct stat stat_buf;

    write(fd, header.c_str(), header.length());
    int fdimg = open(filePath.c_str(), O_RDONLY);

    if(fdimg < 0){
        printf("cannot open file path %s\n", filePath.c_str());
        return;
    }

    fstat(fdimg, &stat_buf);
    int img_total_size = stat_buf.st_size;
    int block_size = stat_buf.st_blksize;

    if(fdimg >= 0){
        size_t sent_size;
        while(img_total_size > 0){
            int send_bytes = ((img_total_size < block_size) ? img_total_size : block_size);
            int done_bytes = sendfile(fd, fdimg, NULL, send_bytes);
        }
    }
}

string findFileExt(string fileEx){
    for(int i = 0; i <= sizeof(fileExtension); i++){
        if(fileExtension[i] == fileEx){
            return ContentType[i];
        }
    }
    printf("serving .%s as html\n", fileEx.c_str());
    return("Content-Type: text/html\r\n\r\n");
}

void getData(string requestType, string client_message){
    string extract;
    string data = client_message;
    
    if(requestType == "GET"){
        data.erase(0, getStr(data, ' ').length() + 1);
        data = getStr(data, ' ');
        data.erase(0, getStr(data, '?').length() + 1);
    } else if(requestType == "POST"){
        int counter = data.length() - 1;
        while(counter > 0){
            if(data[counter] == ' ' || data[counter] == '\n') {break;}
            counter--;
        }
        data.erase(0, counter + 1);
        int found = data.find("=");
        if(found == string::npos){data = "";}
    } 
    int found = client_message.find("cookie");
    if(found != string::npos){
        client_message.erase(0, found + 8);
        client_message = getStr(client_message, ' ');
        data = data + "&" + getStr(client_message, '\n');
    }

    while(data.length() > 0){
        extract = getStr(data, '&');
        serverData.push_back(extract);
        data.erase(0, getStr(data, '&').length() + 1);
    }
}

void *connection_handler(void *socket_desc){
    int newSock = *((int *)socket_desc);
    char client_message[client_message_SIZE];
    int request = read(newSock, client_message, client_message_SIZE);
    string message = client_message;
    sem_wait(&mutex);
    thread_count++;
    printf("thread counter: %d\n", thread_count);
    if(thread_count > 10){
        write(newSock, Messages[BAD_REQUEST].c_str(), Messages[BAD_REQUEST].length());
        thread_count--;
        close(newSock);
        sem_post(&mutex);
        pthread_exit(NULL);
    }
    sem_post(&mutex);

    if(request < 0){
        puts("Receive failed");
    } else if(request == 0){
        puts("Client disconnected unexpectedly");
    } else {
        //printf("Client message: %s\n", client_message);
        string requestType = getStr(message, ' ');
        message.erase(0, requestType.length() + 1);
        string requestFile = getStr(message, ' ');
        
        string requestF = requestFile;
        string fileExt = requestF.erase(0, getStr(requestF, '.').length() + 1);
        string fileEx = getStr(getStr(fileExt, '/'), '?');
        requestFile = getStr(requestFile, '.') + '.' + fileEx;
        
        if(requestType == "GET" || requestType == "POST")
        {
            if(requestFile.length() <= 1){
                requestFile = "/index.html";
            }
            if(fileEx == "php"){
                getData(requestType, client_message);
            }
            sem_wait(&mutex);
            send_message(newSock, requestFile, findFileExt(fileEx));
            sem_post(&mutex);
        } 
    }
    printf("\n ------ exiting server -------\n ");
    close(newSock);
    sem_wait(&mutex);
    thread_count--;
    sem_post(&mutex);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]){

    sem_init(&mutex, 0, 1);
    /* Create TCP socket */
    int server_socket;
    int client_socket;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    /* Check if socket was created */
    if(server_socket < 0){
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
    if(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0){
        perror("socket could not be bind");
        exit(EXIT_FAILURE);
    }
    if(listen(server_socket, 10) < 0){
        perror("Error in listening");
        exit(EXIT_FAILURE);
    }

    /* Listen for connection from client */
    struct sockaddr_in client_address;
    char ip4[INET_ADDRSTRLEN];
    socklen_t len = sizeof(client_address);
    printf("Listening on port: %d\n", PORT);
    while(1){
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &len);
        if(client_socket < 0){
            perror("Unable to accept connection");
            return 1;
        } else {
            inet_ntop(AF_INET, &(client_address.sin_addr), ip4, INET_ADDRSTRLEN);
            printf("Client %s connected", ip4);
        }
        pthread_t multi_thread;
        int *thread_sock = new int();
        *thread_sock = client_socket;

        if(pthread_create(&multi_thread, NULL, connection_handler, (void*)thread_sock) > 0){
            perror("Could not create thread");
            return 0;
        }
    }
}