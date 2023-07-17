#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h> 

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <file_path> <string_to_write>\n", argv[0]);
        return 1;
    }
    openlog("writer", LOG_PID, LOG_USER);
    char *file_path = argv[1];
    char *string_to_write = argv[2];

    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file %s\n", file_path);
        return 1;
    }

    int ret = fprintf(file, "%s", string_to_write);
    if(ret < 0){ //Error case
        syslog(LOG_ERR, "Error writing to file %s", file_path);
        fclose(file);
        closelog();
        return 1;
    } else{ //Success write to file
        syslog(LOG_DEBUG, "Writing %s to %s", string_to_write, file_path);
    }

    fclose(file);
    closelog();

    return 0;
}