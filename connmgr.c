#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#include "connmgr.h"
#include "lib/tcpsock.h"
#include "config.h"

#ifndef TIMEOUT
#define TIMEOUT 5
#endif

typedef struct {
    tcpsock_t *socket;
    sbuffer_t *buffer;
} thread_args_t;

void *client_handler(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    tcpsock_t *client = args->socket;
    sbuffer_t *buffer = args->buffer;
    free(args);

    int sd, result;
    sensor_data_t data;
    int bytes;
    char log_msg[256];
    bool first_packet = true;
    sensor_id_t sensor_id = 0;

    if (tcp_get_sd(client, &sd) == TCP_NO_ERROR) {
        struct timeval tv;
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;
        if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            perror("setsockopt failed");
        }
    }

    while (1) {
        bytes = sizeof(data.id);
        result = tcp_receive(client, (void *)&data.id, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;


        bytes = sizeof(data.value);
        result = tcp_receive(client, (void *)&data.value, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;

        bytes = sizeof(data.ts);
        result = tcp_receive(client, (void *)&data.ts, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;

        if (first_packet) {
            sensor_id = data.id;
            snprintf(log_msg, sizeof(log_msg), "Sensor node %d has opened a new connection", sensor_id);
            write_to_log_process(log_msg);
            first_packet = false;
        }

        sbuffer_insert(buffer, &data);
    }

    if (sensor_id != 0) {
        snprintf(log_msg, sizeof(log_msg), "Sensor node %d has closed the connection", sensor_id);
        write_to_log_process(log_msg);
    } else {
        write_to_log_process("A sensor node closed connection before sending data");
    }

    tcp_close(&client);
    return NULL;
}

void connmgr_listen(int port_number, int max_connections, sbuffer_t *buffer) {
    tcpsock_t *server_socket, *client_socket;
    pthread_t *threads = malloc(sizeof(pthread_t) * max_connections);
    int conn_counter = 0;

    if (tcp_passive_open(&server_socket, port_number) != TCP_NO_ERROR) {
        write_to_log_process("Error: Failed to open server socket");
        free(threads);
        return;
    }
    // printf("Server started on port %d. Waiting for %d connections...\n", port_number, max_connections);

    while (conn_counter < max_connections) {
        if (tcp_wait_for_connection(server_socket, &client_socket) == TCP_NO_ERROR) {

            thread_args_t *args = malloc(sizeof(thread_args_t));
            if (args == NULL) {
                tcp_close(&client_socket);
                continue;
            }
            args->socket = client_socket;
            args->buffer = buffer;

            if (pthread_create(&threads[conn_counter], NULL, client_handler, args) != 0) {
                write_to_log_process("Error: Failed to create thread");
                free(args);
                tcp_close(&client_socket);
            } else {
                conn_counter++;
            }
        } else {
             write_to_log_process("Error: Failed to accept connection");
        }
    }

    tcp_close(&server_socket);

    for (int i = 0; i < conn_counter; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}

void connmgr_free() {

}