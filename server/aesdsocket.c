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
int client_socket_list[MAX_CONNECTIONS] = {0,};
int server_fd = 0;

//Handle signal handler first
void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        printf("Sig interupt or terminate\n");
        for (int i=0; i<MAX_CONNECTIONS; i++) {
            if (client_socket_list[i] != 0) {
                close(client_socket_list[i]);
            }
        }
        remove(FILE_SAVE_DATA);
        close(server_fd);
        exit(1);
    }
}

void start(){

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
    address.sin_addr.s_addr = INADDR_ANY;
    
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
        start();
    } else if(argc == 2 && strcmp(argv[1],"-d")) {
        printf("Daemon mode\n");
        //Need to do sth here @@
    } else {
        printf("Invalid arguments\n");
    }
}