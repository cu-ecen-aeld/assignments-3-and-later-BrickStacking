#include "aesdsocket.h"

//Variable using
int server_fd = 0, client_fd = 0;
char buffer[MAX_PACKET_SIZE];
bool exit_flag = false;
pthread_mutex_t mutex_file;
//Create link-list
struct slist_thread head; /* Singly linked List head. */

//Handle signal handler first
void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting\n");
        printf(stderr, "Caught signal, exiting\n");

    }
}

void error_handler(){
    closelog();
    remove(FILE_SAVE_DATA);
    close(server_fd);
    exit(ERROR_CODE);
}

void* client_handler(void *data){
    if(data != NULL)
    {
        struct server_data* server_data_info = (struct server_data*) data;

        //Open file to save received data
        FILE *file = fopen(FILE_SAVE_DATA, "a+");
        if (file == NULL) {
            syslog(LOG_INFO, "Error open file save data");
            close(client_fd);
            // continue;
        }

        size_t receivedSize = 0;
        memset(buffer, '\0', MAX_PACKET_SIZE);
        pthread_mutex_lock(server_data_info->m_file_mutex);
        while((receivedSize = recv(client_fd, buffer, MAX_PACKET_SIZE, 0)) > 0) {
            //Seeking end of line to write to file
            char *packet_end = strchr(buffer, '\n'); //End of 
            if(packet_end != NULL){ //Seek to end and append
                if(fseek(file, 0, SEEK_END)){
                    printf("Error fseek end");
                    syslog(LOG_INFO, "Error fseek end");
                    exit(ERROR_CODE);
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
        pthread_mutex_unlock(server_data_info->m_file_mutex);
        //Send back data to client
        if(fseek(file, 0, SEEK_SET)){
            printf("Error fseek set");
            syslog(LOG_INFO, "Error fseek set");
            exit(ERROR_CODE);
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

        syslog(LOG_INFO, "Closed connection from %s", server_data_info->m_ip_address_client);
        closelog();
    }

}

void* timer_handler(void *data){
    if(data != NULL)
    {
        struct timestamp_thread* timer_info = (struct server_data*) data;
        struct timespec ts;
        struct tm *tm;
        time_t t;
        char timestr[100] = {'\0',};
        char contentTimestr[200] = {'\0',};
        while(1){
            sleep(10);
            if(exit_flag)
            {
                break;
            }
            tm = localtime(&t);
            if(tm == NULL){
                syslog(LOG_INFO, "Get local time failed");
                exit(ERROR_CODE);
            }
            if(strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %z", tm) == 0){
                syslog(LOG_INFO, "strftime returned 0");
                exit(ERROR_CODE);
            }
            pthread_mutex_lock(timer_info->m_file_mutex);
            FILE *file = fopen(FILE_SAVE_DATA, "a+");
            if (file == NULL) {
                syslog(LOG_INFO, "Error open file save data");
                close(client_fd);
                // continue;
            }
            snprintf(contentTimestr, 200, "timestamp:%s\n", timestr);
            fwrite(contentTimestr, 1, 200, file);
            fclose(file);
            pthread_mutex_unlock(timer_info->m_file_mutex);
        }
    }
}

void start(){

    listen(server_fd, SOCK_STREAM);
    //Init address and socklen
    struct sockaddr_in client_address;
    socklen_t client_socklent = sizeof(client_address);
    //Loop in accept new client, handle each client as 
    while(1){
        if(exit_flag)
        {
            break;
        }
        memset(&client_address, 0, sizeof(struct sockaddr_in));
        client_fd = accept(server_fd, (struct sockaddr *) &client_address, &client_socklent);
        if(client_fd == -1){
            syslog(LOG_INFO, "Client fs is error\n");
            closelog();
            continue;
        }
        struct server_data *new_server_data, *tmp_server_data;
        new_server_data = malloc(sizeof(struct server_data));
        new_server_data->m_send_data_done = false;
        new_server_data->m_file_mutex = &mutex_file;

        //Add to linked-list
        SLIST_INSERT_HEAD(&head, new_server_data, entries);
        pthread_create(&new_server_data->m_thread_id, NULL, client_handler, (void*)new_server_data);


        //Use syslog to print ip of client
        char *client_ip = inet_ntoa(client_address.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
        printf("Accepted connection from %s\n", client_ip);
        closelog();
        memset(new_server_data->m_ip_address_client, 0, sizeof(struct sockaddr_in));
        memcpy(new_server_data->m_ip_address_client, client_ip, sizeof(*client_ip));

        SLIST_FOREACH(tmp_server_data, &head, entries){
            if(tmp_server_data->m_send_data_done == true){
                syslog(LOG_INFO, "Thread:%ld is done\n", tmp_server_data->m_thread_id);
                pthread_join(tmp_server_data->m_thread_id, NULL);
                SLIST_REMOVE(&head, tmp_server_data, server_data, entries);
                syslog(LOG_INFO, "Remove element done from list \n"); 
            }
        }
    }
}

int main(int argc, char **argv) {
    //Handle exeption first
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
     openlog("Server", LOG_PID, LOG_USER);
    SLIST_INIT(&head); /* Initialize the queue. */

    struct timestamp_thread *new_timestamp;
    new_timestamp = malloc(sizeof(struct timestamp_thread));
    new_timestamp->m_file_mutex = &mutex_file;
    pthread_create(&new_timestamp->m_thread_id, NULL, timer_handler, (void*)new_timestamp);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == -1) {
        exit(ERROR_CODE);
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
        exit(ERROR_CODE);
    }
    ret = setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, (char*) & yes, sizeof(int32_t));
    if (ret == -1) {
        printf("Failed to setsockopt SO_REUSEPORT\n");
        exit(ERROR_CODE);

    }
    ret = bind(server_fd, (struct sockaddr *) &address, sizeof(address));
    if (ret == -1) {
        printf("Failed to bind\n");
        exit(ERROR_CODE);
    }

    //Check the mode running
    if (argc == 1) {
        printf("Normal mode\n");
    } else if(argc == 2 && strcmp(argv[1], "-d") == 0) {
        printf("Daemon mode\n");
        pid_t pid = fork();
        if(pid == -1)
        {
            return -1;
        } else if (pid != 0){
            exit(ERROR_CODE);
        }
    } else {
        printf("Invalid arguments\n");
        return 0;
    }
    start();
    error_handler();
    return 0;
}