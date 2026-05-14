#ifndef HISTORY_H
#define HISTORY_H

#include "config.h"

void init_history(HistoryData *history);
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
                    double battery);

#endif
