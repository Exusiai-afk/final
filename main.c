#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"

// --- Logger Implementation ---

static int log_pipe_fd[2];
static pid_t logger_pid = 0;

void logger_loop(int read_fd) {
    FILE *log_file = fopen("gateway.log", "w"); // Req 8: create new empty file
    if (log_file == NULL) {
        perror("Logger: Failed to open gateway.log");
        exit(EXIT_FAILURE);
    }

    FILE *pipe_stream = fdopen(read_fd, "r");
    if (pipe_stream == NULL) {
        perror("Logger: fdopen failed");
        fclose(log_file);
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    int sequence_num = 0;

    while (fgets(buffer, sizeof(buffer), pipe_stream) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;

        time_t now;
        time(&now);
        char *time_str = ctime(&now);
        time_str[strcspn(time_str, "\n")] = 0;

        // Req 8: Format <sequence number> <timestamp> <log-event info message>
        fprintf(log_file, "%d %s %s\n", sequence_num++, time_str, buffer);
        fflush(log_file);
    }

    fclose(pipe_stream);
    fclose(log_file);
    exit(EXIT_SUCCESS);
}

int create_log_process() {
    if (pipe(log_pipe_fd) == -1) {
        perror("Pipe creation failed");
        return -1;
    }
    logger_pid = fork();
    if (logger_pid < 0) {
        perror("Fork failed");
        return -1;
    }
    if (logger_pid == 0) {
        close(log_pipe_fd[1]);
        logger_loop(log_pipe_fd[0]);
        exit(0);
    } else {
        close(log_pipe_fd[0]);
        return 0;
    }
}

int end_log_process() {
    if (logger_pid <= 0) return -1;
    close(log_pipe_fd[1]);
    waitpid(logger_pid, NULL, 0);
    logger_pid = 0;
    return 0;
}

int write_to_log_process(char *msg) {
    if (logger_pid <= 0) return -1;
    char buffer[300];
    snprintf(buffer, sizeof(buffer), "%s\n", msg);
    if (write(log_pipe_fd[1], buffer, strlen(buffer)) == -1) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <max_connections>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int max_conn = atoi(argv[2]);
    sbuffer_t *sbuf;
    pthread_t datamgr_thread, storagemgr_thread;

    if (create_log_process() != 0) {
        fprintf(stderr, "Failed to create log process\n");
        exit(EXIT_FAILURE);
    }

    if (sbuffer_init(&sbuf) != SBUFFER_SUCCESS) {
        fprintf(stderr, "Failed to init sbuffer\n");
        end_log_process();
        exit(EXIT_FAILURE);
    }


    if (pthread_create(&datamgr_thread, NULL, datamgr_run, sbuf) != 0) {
        fprintf(stderr, "Failed to create datamgr thread\n");
        // Cleanup...
    }
    if (pthread_create(&storagemgr_thread, NULL, storage_mgr_run, sbuf) != 0) {
        fprintf(stderr, "Failed to create storagemgr thread\n");
        // Cleanup...
    }

    connmgr_listen(port, max_conn, sbuf);

    sensor_data_t end_marker;
    end_marker.id = 0;
    sbuffer_insert(sbuf, &end_marker);

    pthread_join(datamgr_thread, NULL);
    pthread_join(storagemgr_thread, NULL);

    sbuffer_free(&sbuf);
    end_log_process();

    return 0;
}