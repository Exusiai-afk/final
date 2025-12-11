#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "datamgr.h"
#include "lib/dplist.h"
#include "logger.h"
#include "config.h"

#ifndef SET_MIN_TEMP
#define SET_MIN_TEMP 10
#endif

#ifndef SET_MAX_TEMP
#define SET_MAX_TEMP 20
#endif

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

typedef struct {
    uint16_t sensor_id;
    uint16_t room_id;
    double running_avg;
    time_t last_modified;
    double readings[RUN_AVG_LENGTH];
    int read_index;
    int count;
} my_element_t;

// --- Dplist Callback Functions ---
void *element_copy(void *element) {
    my_element_t *copy = malloc(sizeof(my_element_t));
    *copy = *(my_element_t *)element; // 浅拷贝即可，因为没有指针成员
    return (void *)copy;
}

void element_free(void **element) {
    free(*element);
    *element = NULL;
}

int element_compare(void *x, void *y) {
    return ((((my_element_t *)x)->sensor_id < ((my_element_t *)y)->sensor_id) ? -1 : 
            (((my_element_t *)x)->sensor_id == ((my_element_t *)y)->sensor_id) ? 0 : 1);
}

// --- Helper Functions ---
void update_running_avg(my_element_t *sensor, double new_value) {
    // 插入新值
    sensor->readings[sensor->read_index] = new_value;
    sensor->read_index = (sensor->read_index + 1) % RUN_AVG_LENGTH;
    if (sensor->count < RUN_AVG_LENGTH) {
        sensor->count++;
    }

    double sum = 0;
    for (int i = 0; i < sensor->count; i++) {
        sum += sensor->readings[i];
    }
    sensor->running_avg = sum / sensor->count;
}

// --- Main Thread Function ---
void *datamgr_run(void *arg) {
    sbuffer_t *buffer = (sbuffer_t *)arg;
    dplist_t *sensor_list = NULL;
    FILE *map_file;
    sensor_data_t data;
    char log_msg[256];
    int result;

    sensor_list = dpl_create(element_copy, element_free, element_compare);

    map_file = fopen("room_sensor.map", "r");
    if (map_file == NULL) {
        write_to_log_process("Error: Could not open room_sensor.map");
    } else {
        uint16_t room_id, sensor_id;
        while (fscanf(map_file, "%hu %hu", &room_id, &sensor_id) == 2) {
            my_element_t new_sensor;
            new_sensor.sensor_id = sensor_id;
            new_sensor.room_id = room_id;
            new_sensor.running_avg = 0;
            new_sensor.last_modified = 0;
            new_sensor.read_index = 0;
            new_sensor.count = 0;
            // 插入列表
            dpl_insert_at_index(sensor_list, &new_sensor, dpl_size(sensor_list), true); // Insert copy
        }
        fclose(map_file);
    }

    while (1) {
        result = sbuffer_remove(buffer, &data, READER_DATAMGR);

        if (result == SBUFFER_NO_DATA) {
            break;
        }

        my_element_t search_dummy;
        search_dummy.sensor_id = data.id;
        int index = dpl_get_index_of_element(sensor_list, &search_dummy);

        if (index == -1) {
            snprintf(log_msg, sizeof(log_msg), "Received sensor data with invalid sensor node ID %d", data.id);
            write_to_log_process(log_msg);
        } else {
            my_element_t *sensor = (my_element_t *)dpl_get_element_at_index(sensor_list, index);

            sensor->last_modified = data.ts;

            update_running_avg(sensor, data.value);

            if (sensor->count >= RUN_AVG_LENGTH) {
                if (sensor->running_avg < SET_MIN_TEMP) {
                    snprintf(log_msg, sizeof(log_msg), "Sensor node %d reports it's too cold (avg temp = %.2f)", sensor->sensor_id, sensor->running_avg);
                    write_to_log_process(log_msg);
                } else if (sensor->running_avg > SET_MAX_TEMP) {
                    snprintf(log_msg, sizeof(log_msg), "Sensor node %d reports it's too hot (avg temp = %.2f)", sensor->sensor_id, sensor->running_avg);
                    write_to_log_process(log_msg);
                }
            }
        }
    }
    dpl_free(&sensor_list, true);
    return NULL;
}

void datamgr_free() {

}