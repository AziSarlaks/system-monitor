#ifndef APP_STATE_H
#define APP_STATE_H

#include <gtk/gtk.h>

#include "config.h"

#define APP_TITLE "System Monitor"
#define PROCESS_PREVIEW_ROWS 10
#define ALERT_CPU_TEMP_C 85.0
#define ALERT_GPU_TEMP_C 85.0
#define ALERT_HIGH_LOAD_PERCENT 90.0
#define ALERT_LOAD_RECOVERY_PERCENT 75.0
#define ALERT_LOW_BATTERY_PERCENT 15.0
#define ALERT_BATTERY_RECOVERY_PERCENT 25.0
#define ALERT_LOAD_REQUIRED_SAMPLES 3
#define ALERT_NOTIFICATION_EXPIRE_MS 7000

typedef struct {
    int active;
    int samples;
} AlertState;

typedef struct {
    GPUSource gpu_source;
    guint generation;
    gboolean cpu_ok;
    gboolean memory_ok;
    gboolean gpu_ok;
    gboolean disk_ok;
    gboolean network_ok;
    gboolean battery_ok;
    gboolean sensor_ok;
    gboolean processes_ok;
    CPUStats cpu;
    CPUStats cores[MAX_CORES];
    int cores_count;
    MemoryInfo memory;
    GPUInfo gpu;
    DiskInfo disk;
    NetworkInfo network;
    BatteryInfo battery;
    SensorInfo sensor;
    ProcessInfo processes[MAX_PROCESSES];
    int process_count;
    double gpu_memory_percent;
} MetricsSnapshot;

typedef struct {
    GtkWidget *card;
    GtkWidget *drawing_area;
    GtkWidget *value_label;
    GtkWidget *detail_label;
    double value;
    gboolean dark_theme;
    GdkRGBA color;
} MetricCard;

typedef struct {
    GtkWidget *window;
    GtkWidget *status_label;
    GtkWidget *cores_box;
    GtkWidget *history_area;
    GtkWidget *io_history_area;
    GtkWidget *disk_detail_label;
    GtkWidget *network_detail_label;
    GtkWidget *battery_detail_label;
    GtkWidget *sensor_detail_label;
    GtkWidget *process_count_label;
    GtkWidget *process_window;
    GtkWidget *process_window_count_label;
    GtkWidget *process_search_entry;
    GtkWidget *process_sort_combo;
    GtkWidget *process_refresh_button;
    GtkWidget *process_kill_button;
    GtkWidget *process_force_kill_button;
    GtkWidget *process_window_view;
    GtkWidget *gpu_integrated_button;
    GtkWidget *gpu_nvidia_button;
    GtkWidget *theme_light_button;
    GtkWidget *theme_dark_button;
    GtkWidget *refresh_combo;
    GtkStatusIcon *tray_icon;
    GDBusConnection *notification_bus;
    GtkListStore *process_store;
    GtkListStore *process_window_store;

    MetricCard cpu_card;
    MetricCard memory_card;
    MetricCard gpu_card;

    CPUStats cpu_prev;
    CPUStats cpu_curr;
    CPUStats cores_prev[MAX_CORES];
    CPUStats cores_curr[MAX_CORES];
    int cores_count;
    gboolean has_cpu_sample;

    MemoryInfo memory;
    GPUInfo gpu;
    DiskInfo disk;
    NetworkInfo network;
    BatteryInfo battery;
    SensorInfo sensor;
    GPUSource gpu_source;
    HistoryData history;
    ProcessInfo last_processes[MAX_PROCESSES];
    int last_process_count;
    char process_filter[128];
    int process_sort_mode;
    gboolean dark_theme;
    int refresh_interval_ms;
    int window_width;
    int window_height;
    gboolean metrics_refresh_running;
    guint metrics_generation;
    AlertState cpu_temp_alert;
    AlertState gpu_temp_alert;
    AlertState battery_low_alert;
    AlertState cpu_load_alert;
    AlertState memory_load_alert;
    AlertState gpu_load_alert;

    guint refresh_timer_id;
} AppState;

enum {
    COL_PID,
    COL_NAME,
    COL_STATE,
    COL_CPU,
    COL_MEMORY,
    COL_COMMAND,
    N_COLUMNS
};

#endif
