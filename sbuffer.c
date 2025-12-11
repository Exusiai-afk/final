/**
 * \author {MINGHAO CHEN}
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sbuffer.h"

typedef struct sbuffer_node {
    struct sbuffer_node *next;
    sensor_data_t data;
} sbuffer_node_t;

struct sbuffer {
    sbuffer_node_t *head;
    sbuffer_node_t *tail;

    sbuffer_node_t *last_read_datamgr;
    sbuffer_node_t *last_read_storagemgr;

    pthread_mutex_t mutex;
    pthread_cond_t can_read;
    int end_of_stream;
};


static sbuffer_node_t* create_node() {
    sbuffer_node_t* node = malloc(sizeof(sbuffer_node_t));
    if(node) node->next = NULL;
    return node;
}

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;

    sbuffer_node_t *dummy = create_node();
    if (dummy == NULL) { free(*buffer); return SBUFFER_FAILURE; }

    (*buffer)->head = dummy;
    (*buffer)->tail = dummy;
    (*buffer)->last_read_datamgr = dummy;
    (*buffer)->last_read_storagemgr = dummy;
    (*buffer)->end_of_stream = 0;

    if (pthread_mutex_init(&(*buffer)->mutex, NULL) != 0) {
        free(dummy); free(*buffer); return SBUFFER_FAILURE;
    }
    if (pthread_cond_init(&(*buffer)->can_read, NULL) != 0) {
        pthread_mutex_destroy(&(*buffer)->mutex);
        free(dummy); free(*buffer); return SBUFFER_FAILURE;
    }
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    if ((buffer == NULL) || (*buffer == NULL)) return SBUFFER_FAILURE;

    pthread_mutex_lock(&(*buffer)->mutex);
    sbuffer_node_t *current = (*buffer)->head;
    while (current) {
        sbuffer_node_t *next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&(*buffer)->mutex);

    pthread_mutex_destroy(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->can_read);
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data, int reader_id) {
    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&buffer->mutex);

    sbuffer_node_t **my_cursor = (reader_id == READER_DATAMGR) ?
                                 &buffer->last_read_datamgr :
                                 &buffer->last_read_storagemgr;

    while ((*my_cursor)->next == NULL) {
        if (buffer->end_of_stream) {
            pthread_mutex_unlock(&buffer->mutex);
            return SBUFFER_NO_DATA;
        }
        pthread_cond_wait(&buffer->can_read, &buffer->mutex);
    }

    sbuffer_node_t *next_node = (*my_cursor)->next;
    *data = next_node->data;
    *my_cursor = next_node;

    while (buffer->head != buffer->tail &&
           buffer->head != buffer->last_read_datamgr &&
           buffer->head != buffer->last_read_storagemgr) {

        sbuffer_node_t *garbage = buffer->head;
        buffer->head = buffer->head->next; // Head 后移
        free(garbage);
    }

    pthread_mutex_unlock(&buffer->mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&buffer->mutex);

    if (data->id == 0) {
        buffer->end_of_stream = 1;
        pthread_cond_broadcast(&buffer->can_read);
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_SUCCESS;
    }

    sbuffer_node_t *new_node = create_node();
    if (new_node == NULL) {
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_FAILURE;
    }
    new_node->data = *data;

    buffer->tail->next = new_node;
    buffer->tail = new_node;

    pthread_cond_broadcast(&buffer->can_read);
    pthread_mutex_unlock(&buffer->mutex);

    return SBUFFER_SUCCESS;
}