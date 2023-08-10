#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 9000
#define FILE_SAVE_DATA "/var/tmp/aesdsocketdata"
#define MAX_PACKET_SIZE 1024
#define MAX_CONNECTIONS 10

//Variable using
int server_fd = 0, client_fd = 0;
char buffer[MAX_PACKET_SIZE];

//Handle signal handler first
void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        openlog("Server", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "Caught signal, exiting\n");
        closelog();
        remove(FILE_SAVE_DATA);
        close(server_fd);
        exit(1);
    }
}

void start(){

    listen(server_fd, SOCK_STREAM);
    //Init address and socklen
    struct sockaddr_in client_address;
    socklen_t client_socklent = sizeof(client_address);
    memset(&client_address, 0, sizeof(struct sockaddr_in));
    //Loop in accept new client, handle each client as 
    while(1){
        client_fd = accept(server_fd, (struct sockaddr *) &client_address, &client_socklent);
        if(client_fd == -1){
            openlog("Server", LOG_PID, LOG_USER);
            syslog(LOG_INFO, "Client fs is error\n");
            closelog();
            continue;
        }
        //Use syslog to print ip of client
        char *client_ip = inet_ntoa(client_address.sin_addr);
        openlog("Server", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
        printf("Accepted connection from %s\n", client_ip);
        closelog();

        //Open file to save received data
        FILE *file = fopen(FILE_SAVE_DATA, "a+");
        if (file == NULL) {
            syslog(LOG_INFO, "Error open file save data");
            close(client_fd);
            continue;
        }

        size_t receivedSize = 0;
        memset(buffer, '\0', MAX_PACKET_SIZE);
        while((receivedSize = recv(client_fd, buffer, MAX_PACKET_SIZE, 0)) > 0) {
            //Seeking end of line to write to file
            char *packet_end = strchr(buffer, '\n'); //End of 
            if(packet_end != NULL){ //Seek to end and append
                if(fseek(file, 0, SEEK_END)){
                    printf("Error fseek end");
                    syslog(LOG_INFO, "Error fseek end");
                    return -1;
                }
                size_t packet_size = packet_end - buffer + 1;
                fwrite(buffer, packet_size, 1, file);
                break;
            } else{ //Still have data but not all, temporary put to file
                fputs(buffer, file);
            }
        }
        printf("Data record in buffer:%s", buffer);
        fsync(fileno(file)); //Sync after write to file
        //Send back data to client
        if(fseek(file, 0, SEEK_SET)){
            printf("Error fseek set");
            syslog(LOG_INFO, "Error fseek set");
            return -1;
        }
        size_t sendSize = 0;
        while((sendSize=fread(buffer, sizeof(char), 1024, file))>0){
            printf("Data sending:%s", buffer);
            int ret =  send(client_fd, buffer, sendSize, 0);
            if(ret <= 0){
                printf("Sending back client failed");
                syslog(LOG_INFO, "Sending back client failed");
            }
        }

        fclose(file);
        close(client_fd);

        openlog("Server", LOG_PID, LOG_USER);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        closelog();
    }
}

int main(int argc, char **argv) {
    //Handle exeption first
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        exit(-1);
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(9000);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int32_t yes=1;
    int ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*) & yes, sizeof(int32_t));
    if (ret == -1) {
        printf("Failed to setsockopt SO_REUSEADDR\n");
        exit(-1);
    }
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, (char*) & yes, sizeof(int32_t));
    if (ret == -1) {
        printf("Failed to setsockopt SO_REUSEPORT\n");
        exit(-1);

    }
    ret = bind(server_fd, (struct sockaddr *) &address, sizeof(address));
    if (ret == -1) {
        printf("Failed to bind\n");
        exit(-1);
    }

    //Check the mode running
    if (argc == 1) {
        printf("Normal mode\n");
    } else if(argc == 2 && strcmp(argv[1],"-d")) {
        printf("Daemon mode\n");
        pid_t pid = fork();
        if(pid == -1)
        {
            return -1;
        } else if (pid != 0){
            exit(EXIT_SUCCESS);
        }
    } else {
        printf("Invalid arguments\n");
        return 0;
    }
    start();
}