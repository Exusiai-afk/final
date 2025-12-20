#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sensor_db.h"
#include "config.h"
#include "sbuffer.h"

void *storage_mgr_run(void *arg) {
    sbuffer_t *buffer = (sbuffer_t *)arg;
    sensor_data_t data;
    int result;
    char log_msg[128];
    FILE *csv_file;

    csv_file = fopen("data.csv", "w");
    if (csv_file == NULL) {
        write_to_log_process("Error: Could not create data.csv");
        return NULL;
    }

    write_to_log_process("A new data.csv file has been created");

    while (1) {
        result = sbuffer_remove(buffer, &data, READER_STORAGEMGR);

        if (result == SBUFFER_NO_DATA) {
            break;
        }

        fprintf(csv_file, "%hu,%.4f,%ld\n", data.id, data.value, data.ts);
        fflush(csv_file);

        // Log message: Data insertion from sensor <sensorNodeID> succeeded.
        snprintf(log_msg, sizeof(log_msg), "Data insertion from sensor %d succeeded", data.id);
        write_to_log_process(log_msg);
    }
    
    fclose(csv_file);

    write_to_log_process("The data.csv file has been closed");

    return NULL;
}