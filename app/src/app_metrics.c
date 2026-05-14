#include "app_metrics.h"

#include <stdio.h>
#include <strings.h>

double app_clamp_double(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

double app_clamp_percent(double value) {
    return app_clamp_double(value, 0.0, 100.0);
}

void app_calculate_cpu_usage(const CPUStats *prev, CPUStats *curr) {
    if (!prev || !curr) return;

    double total_diff = curr->total - prev->total;
    double idle_diff = curr->idle - prev->idle;

    if (total_diff > 0.0) {
        curr->usage_percent = app_clamp_percent(100.0 * (1.0 - (idle_diff / total_diff)));
    } else {
        curr->usage_percent = 0.0;
    }
}

void app_format_bytes(char *buffer, size_t size, unsigned long long bytes) {
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;

    if (!buffer || size == 0) {
        return;
    }

    if (bytes >= (unsigned long long)gib) {
        snprintf(buffer, size, "%.1f GB", bytes / gib);
    } else if (bytes >= (unsigned long long)mib) {
        snprintf(buffer, size, "%.1f MB", bytes / mib);
    } else {
        snprintf(buffer, size, "%llu KB", bytes / 1024ULL);
    }
}

int app_visible_process_rows(int process_count, int max_rows) {
    if (process_count < 0) {
        return 0;
    }

    if (max_rows <= 0 || process_count < max_rows) {
        return process_count;
    }

    return max_rows;
}

int app_alert_update_high(double value,
                          double trigger_threshold,
                          double recovery_threshold,
                          int required_samples,
                          int *active,
                          int *samples) {
    if (!active || !samples) {
        return 0;
    }
    if (required_samples < 1) {
        required_samples = 1;
    }

    if (*active) {
        if (value <= recovery_threshold) {
            *active = 0;
            *samples = 0;
        }
        return 0;
    }

    if (value >= trigger_threshold) {
        (*samples)++;
        if (*samples >= required_samples) {
            *active = 1;
            return 1;
        }
    } else {
        *samples = 0;
    }

    return 0;
}

int app_alert_update_low(double value,
                         double trigger_threshold,
                         double recovery_threshold,
                         int required_samples,
                         int *active,
                         int *samples) {
    if (!active || !samples) {
        return 0;
    }
    if (required_samples < 1) {
        required_samples = 1;
    }

    if (*active) {
        if (value >= recovery_threshold) {
            *active = 0;
            *samples = 0;
        }
        return 0;
    }

    if (value <= trigger_threshold) {
        (*samples)++;
        if (*samples >= required_samples) {
            *active = 1;
            return 1;
        }
    } else {
        *samples = 0;
    }

    return 0;
}

int app_battery_status_can_alert(const BatteryInfo *battery) {
    if (!battery || !battery->present) {
        return 0;
    }

    return strcasecmp(battery->status, "Charging") != 0 &&
           strcasecmp(battery->status, "Full") != 0;
}
