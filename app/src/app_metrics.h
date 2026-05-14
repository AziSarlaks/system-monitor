#ifndef APP_METRICS_H
#define APP_METRICS_H

#include <stddef.h>

#include "config.h"

double app_clamp_double(double value, double min, double max);
double app_clamp_percent(double value);
void app_calculate_cpu_usage(const CPUStats *prev, CPUStats *curr);
void app_format_bytes(char *buffer, size_t size, unsigned long long bytes);
int app_visible_process_rows(int process_count, int max_rows);
int app_alert_update_high(double value,
                          double trigger_threshold,
                          double recovery_threshold,
                          int required_samples,
                          int *active,
                          int *samples);
int app_alert_update_low(double value,
                         double trigger_threshold,
                         double recovery_threshold,
                         int required_samples,
                         int *active,
                         int *samples);
int app_battery_status_can_alert(const BatteryInfo *battery);

#endif
