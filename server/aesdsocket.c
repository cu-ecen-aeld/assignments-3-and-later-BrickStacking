#include "aesdsocket.h"

// Variable using
int g_server_fd = 0, g_client_fd = 0;
bool g_exit_flag = false;
int g_data_file_fd = 0; // fd of file save data
pthread_mutex_t g_mutex_file;
// Struct for timer

timer_t timer_id;

// Create link-list
struct server_data *tmp_data_server = NULL;
struct server_data *loop = NULL;
SLIST_HEAD(slisthead, server_data) head; /* Singly linked List head. */

// Handle signal handler first
void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting\n");
        perror("Caught signal, exiting\n");
        shutdown(g_server_fd, SHUT_RDWR);
        g_exit_flag = true;
    }
}

void error_handler()
{
    g_exit_flag = true;
    close(g_server_fd);
    close(g_data_file_fd);
    remove(FILE_SAVE_DATA);

    // Stop thread running
    SLIST_FOREACH(tmp_data_server, &head, entries)
    {
        if (tmp_data_server->m_send_data_done == false)
        { // Still running, -> Cancel running thread
            pthread_cancel(tmp_data_server->m_thread_id);
        }
    }
    // Free all the node in linked-list
    while (!SLIST_EMPTY(&head))
    {
        tmp_data_server = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        free(tmp_data_server);
    }

    pthread_mutex_destroy(&g_mutex_file);
    timer_delete(timer_id);
    closelog();
}

// Addition function use to print "Timestamp:time" to file each 10 second
void timer_handler(union sigval param)
{
    struct timestamp_struct *time_data = (struct timestamp_struct *)param.sival_ptr;
    char t_buf[MAX_TIME_STRING_SIZE];
    time_t current_time;
    struct tm *local_time;
    int byte_counter;

    // Get time first
    if (-1 == time(&current_time))
    {
        perror("Failed to get time()");
        error_handler();
        exit(ERROR_CODE);
    }

    // Get local time
    local_time = localtime(&current_time);
    if (NULL == local_time)
    {
        perror("Local time NULL");
        error_handler();
        exit(ERROR_CODE);
    }

    // Get time with format to buffer
    byte_counter = strftime(t_buf, MAX_TIME_STRING_SIZE, "timestamp:%a, %d %b %Y %T %z\n", local_time);
    if (0 == byte_counter)
    {
        perror("strftime failed");
        error_handler();
        exit(ERROR_CODE);
    }

    // Write to file with mutex lock
    if (-1 == pthread_mutex_lock(time_data->m_file_mutex))
    {
        perror("Unable to mutex lock");
        error_handler();
        exit(ERROR_CODE);
    }

    if (-1 == write(time_data->data_file_fd, t_buf, byte_counter))
    {
        perror("Failed to write to file descriptor");
        error_handler();
        exit(ERROR_CODE);
    }

    if (-1 == pthread_mutex_unlock(time_data->m_file_mutex))
    {
        perror("Unable to mutex lock");
        error_handler();
        exit(ERROR_CODE);
    }
}


void* thread_handler(void *arg)
{
    int receivedSize = 0, totalReceive = 0;
    int pos = 0;
    int extra_alloc = 1;
    struct server_data *data_server = (struct server_data *)arg;
    // Init buffer receive and send back
    data_server->data_receive = calloc(MAX_PACKET_SIZE, sizeof(char));
    // data_server->data_send_back = calloc(MAX_PACKET_SIZE, sizeof(char));
    // Receive data from client
    while ((receivedSize = recv(data_server->m_client_fd, data_server->data_receive + pos, MAX_PACKET_SIZE, 0)) > 0)
    {
        if(-1 == receivedSize)
        {
            perror("resv()");
            error_handler();
            exit(ERROR_CODE);          
        } 
        // Add offset to pos after receiving data
        pos += receivedSize;
        // When receive end of line, break to send back data to client
        if (strchr(data_server->data_receive, '\n') != NULL)
        {
            break;
        }
        extra_alloc++;
        data_server->data_receive = (char*)realloc(data_server->data_receive,(extra_alloc*MAX_PACKET_SIZE)*sizeof(char));
        if(NULL == data_server->data_receive)
        {
            syslog(LOG_ERR,"Error: realloc()");
            free(data_server->data_receive);
            error_handler();
            exit(ERROR_CODE);
        }
        }
    // Use mutex to write file
    if (-1 == pthread_mutex_lock(data_server->m_file_mutex))
    {
        error_handler();
        exit(ERROR_CODE);
    }

    // Write to fd
    if (-1 == write(data_server->data_file_fd, data_server->data_receive, pos))
    {
        error_handler();
        exit(ERROR_CODE);
    }
    if(-1 == pthread_mutex_unlock(data_server->m_file_mutex))
    {
        error_handler();
        exit(ERROR_CODE);
    }    

    // // Alloc buffer send back to client
    totalReceive = lseek(data_server->data_file_fd, 0, SEEK_END);
    lseek(data_server->data_file_fd, 0, SEEK_SET);
    // Alloc memory
    data_server->data_send_back = calloc(totalReceive, sizeof(char));
    // Read from file fd
    if (-1 == pthread_mutex_lock(data_server->m_file_mutex))
    {
        error_handler();
        exit(ERROR_CODE);
    }
    int totalRead = read(data_server->data_file_fd, data_server->data_send_back, totalReceive);
    if (-1 == totalRead)
    {
        error_handler();
        exit(ERROR_CODE);
    }
    // Send back to client
    if (-1 == send(data_server->m_client_fd, data_server->data_send_back, totalRead, 0))
    {
        error_handler();
        exit(ERROR_CODE);
    }
    if (-1 == pthread_mutex_unlock(data_server->m_file_mutex))
    {
        error_handler();
        exit(ERROR_CODE);
    }
    data_server->m_send_data_done = true;
    close(data_server->m_client_fd);
    free(data_server->data_receive);
    free(data_server->data_send_back);
    return arg;
}


int main(int argc, char **argv)
{
    openlog("Server", LOG_PID, LOG_USER);
    // Handle exeption first
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    struct itimerspec itimer;
    struct timespec start_time;
    SLIST_INIT(&head); /* Initialize the queue. */
    if(pthread_mutex_init(&g_mutex_file,NULL) != 0)
    {
        error_handler();
        exit(ERROR_CODE);
    } 
    // Set up socket
    g_server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_server_fd == -1)
    {
        perror("Socket failed\n");
        exit(ERROR_CODE);
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(9000);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    int32_t yes = 1;
    int ret = setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int32_t));
    if (ret == -1)
    {
        perror("Failed to setsockopt SO_REUSEADDR\n");
        close(g_server_fd);
        exit(ERROR_CODE);
    }

    ret = setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEPORT, (char *)&yes, sizeof(int32_t));
    if (ret == -1)
    {
        perror("Failed to setsockopt SO_REUSEPORT\n");
        close(g_server_fd);
        exit(ERROR_CODE);
    }

    ret = bind(g_server_fd, (struct sockaddr *)&address, sizeof(address));
    if (ret == -1)
    {
        perror("Failed to bind\n");
        close(g_server_fd);
        exit(ERROR_CODE);
    }

    if (-1 == listen(g_server_fd, MAX_CONNECTIONS)) // backlog assumed 10 i.e, >1
    {
        perror("Failed to listen\n");
        close(g_server_fd);
        exit(ERROR_CODE);
    }

    // Create file fd to write data
    g_data_file_fd = open(FILE_SAVE_DATA, O_CREAT | O_RDWR, 0777);
    if (-1 == g_data_file_fd)
    {
        perror("Failed to create fd\n");
        close(g_server_fd);
        exit(ERROR_CODE);
    }
    // Check the mode running
    if (argc == 1)
    {
        perror("Normal mode\n");
    }
    else if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        pid_t pid;
        pid = fork();
        if (-1 == pid)
        {
            perror("Failed to fork\n");
            exit(ERROR_CODE);
        }
        else if (pid != 0)
        {
            exit(EXIT_SUCCESS);
        }

        // Create new session of process after fork
        if (-1 == setsid())
        {
            perror("Failed to setsid\n");
            exit(ERROR_CODE);
        }
        // Redirect output
        open("/dev/null", O_RDWR);
        (void)dup(0);
        (void)dup(1);
    }
    else
    {
        perror("Invalid arguments\n");
        return -1;
    }

    //Setup timer
    struct timestamp_struct timer_data;
    timer_data.data_file_fd = g_data_file_fd;
    timer_data.m_file_mutex = &g_mutex_file;
    struct sigevent timer_event_handler;
    memset(&timer_event_handler, 0, sizeof(struct sigevent));
    timer_event_handler.sigev_notify = SIGEV_THREAD;
    timer_event_handler.sigev_value.sival_ptr = &timer_data;
    timer_event_handler.sigev_notify_function = timer_handler;
    // timer calls the handler for every 10secs

    // Setup time
    if (timer_create(CLOCK_MONOTONIC, &timer_event_handler, &timer_id) != 0)
    {
        perror("timer_create()\n");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0)
    {
        perror("clock_gettime()\n");
    }
    itimer.it_value.tv_sec = 10;
    itimer.it_value.tv_nsec = 0;
    itimer.it_interval.tv_sec = 10;
    itimer.it_interval.tv_nsec = 0;  
    if (timer_settime(timer_id, TIMER_ABSTIME, &itimer, NULL) != 0)
    {
        perror("settime error\n");
    }
    // Loop to accept new connection -> create thread to handle incomming data

    // Init address and socklen
    struct sockaddr_in client_address;
    socklen_t client_socklenth = sizeof(client_address);
    
    while (1)
    {
        memset(&client_address, 0, sizeof(struct sockaddr_in));
        // Acept new client
        g_client_fd = accept(g_server_fd, (struct sockaddr *)&client_address, &client_socklenth);
        if (g_client_fd == -1)
        {
            syslog(LOG_INFO, "Client fs is error\n");
            error_handler();
            exit(ERROR_CODE);
        }

        if(g_exit_flag)
        {
            break;
        }
        
        //Print IP address of client
        char *client_ip = inet_ntoa(client_address.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        // perror("Accepted connection from %s\n", client_ip);
        // Use only one tmp pointer to hold address, after add it to linked-list, we can assign tmp pointer to hold another address
        tmp_data_server = (struct server_data*)malloc(sizeof(struct server_data));
        // Add to linked-list
        tmp_data_server->m_send_data_done = false;
        tmp_data_server->m_file_mutex = &g_mutex_file;
        tmp_data_server->data_file_fd = g_data_file_fd;
        tmp_data_server->m_client_fd = g_client_fd;
        SLIST_INSERT_HEAD(&head, tmp_data_server, entries);

        // Create a thread to handle incomming data
        //create the thread
        if(pthread_create(&(tmp_data_server->m_thread_id),NULL,thread_handler,(void*)(tmp_data_server)) != 0){
            perror("Create thread handler failed\n");
            error_handler();
            exit(ERROR_CODE);
        }
        // Interate through all node, if thread done, join it and free the memory
        SLIST_FOREACH_SAFE(tmp_data_server, &head, entries, loop)
        {
            if (pthread_join(tmp_data_server->m_thread_id, NULL) != 0)
            {
                perror("Join failed \n");
                error_handler();
                exit(ERROR_CODE);
            }
            if (true == tmp_data_server->m_send_data_done)
            {
                SLIST_REMOVE(&head, tmp_data_server, server_data, entries);
                free(tmp_data_server);
                tmp_data_server = NULL;
            }
        }
    }
    error_handler();
    return 0;
}