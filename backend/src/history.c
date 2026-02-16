#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "history.h"

void init_history(HistoryData *history) {
    memset(history, 0, sizeof(HistoryData));
    history->index = 0;
    history->count = 0;
}

void add_to_history(HistoryData *history, double cpu_usage, double memory_usage, 
                    double gpu_usage, double gpu_memory, double gpu_temp) {
    time_t now = time(NULL);
    
    history->cpu_usage[history->index] = cpu_usage;
    history->memory_usage[history->index] = memory_usage;
    history->gpu_usage[history->index] = gpu_usage;
    history->gpu_memory[history->index] = gpu_memory;
    history->gpu_temperature[history->index] = gpu_temp;
    history->timestamps[history->index] = now;
    
    history->index = (history->index + 1) % HISTORY_SIZE;
    if (history->count < HISTORY_SIZE) {
        history->count++;
    }
}

void get_history_json(char *buffer, int buffer_size, HistoryData *history) {
    int offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "{\n"
        "  \"cpu\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%.1f", history->cpu_usage[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"memory\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%.1f", history->memory_usage[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"gpu\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%.1f", history->gpu_usage[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"gpu_memory\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%.1f", history->gpu_memory[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"gpu_temperature\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%.1f", history->gpu_temperature[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"timestamps\": [");
    
    for (int i = 0; i < history->count; i++) {
        int idx = (history->index - history->count + i + HISTORY_SIZE) % HISTORY_SIZE;
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset, "%ld", history->timestamps[idx]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset,
        "],\n"
        "  \"count\": %d\n"
        "}", history->count);
    
    if (offset >= buffer_size) {
        buffer[buffer_size - 1] = '\0';
    }
}