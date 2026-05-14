#include <string.h>
#include <time.h>
#include "config.h"
#include "history.h"

void init_history(HistoryData *history) {
    memset(history, 0, sizeof(HistoryData));
    history->index = 0;
    history->count = 0;
}

void add_to_history(HistoryData *history,
                    double cpu_usage,
                    double memory_usage,
                    double gpu_usage,
                    double gpu_memory,
                    double gpu_temp,
                    double disk_read,
                    double disk_write,
                    double network_rx,
                    double network_tx,
                    double battery) {
    time_t now = time(NULL);
    
    history->cpu_usage[history->index] = cpu_usage;
    history->memory_usage[history->index] = memory_usage;
    history->gpu_usage[history->index] = gpu_usage;
    history->gpu_memory[history->index] = gpu_memory;
    history->gpu_temperature[history->index] = gpu_temp;
    history->disk_read[history->index] = disk_read;
    history->disk_write[history->index] = disk_write;
    history->network_rx[history->index] = network_rx;
    history->network_tx[history->index] = network_tx;
    history->battery[history->index] = battery;
    history->timestamps[history->index] = now;
    
    history->index = (history->index + 1) % HISTORY_SIZE;
    if (history->count < HISTORY_SIZE) {
        history->count++;
    }
}
