#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/queue.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define PORT 9000
#define FILE_SAVE_DATA "/var/tmp/aesdsocketdata"
#define MAX_PACKET_SIZE 1024
#define MAX_TIME_STRING_SIZE 100
#define MAX_CONNECTIONS 10
#define MAX_IP_LENGTH   20
#define MAX_DELAY_TIME  10
#define ERROR_CODE -1


struct server_data {
    pthread_t m_thread_id;
    pthread_mutex_t *m_file_mutex;
    int m_client_fd;
    int data_file_fd;
    bool m_send_data_done;
    char *data_receive;
    char *data_send_back;
    SLIST_ENTRY(server_data) entries;
};

struct timestamp_struct {
    pthread_mutex_t *m_file_mutex;
    int data_file_fd;
};