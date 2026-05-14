#ifndef APP_METRICS_H
#define APP_METRICS_H

#include <stddef.h>

#include "config.h"

double app_clamp_double(double value, double min, double max);
double app_clamp_percent(double value);
void app_calculate_cpu_usage(const CPUStats *prev, CPUStats *curr);
void app_format_bytes(char *buffer, size_t size, unsigned long long bytes);
int app_visible_process_rows(int process_count, int max_rows);

#endif
