#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PROCESSES 512
#define UPDATE_INTERVAL_MS 2000
#define MAX_CORES 32
#define HISTORY_SIZE 60

typedef enum {
    GPU_SOURCE_INTEGRATED = 0,
    GPU_SOURCE_NVIDIA = 1
} GPUSource;

typedef struct {
    unsigned long long total;
    unsigned long long used;
    unsigned long long free;
    unsigned long long cached;
    double percentage;
} MemoryInfo;

typedef struct {
    double user;
    double nice;
    double system;
    double idle;
    double iowait;
    double irq;
    double softirq;
    double steal;
    double guest;
    double guest_nice;
    double total;
    double usage_percent;
    double temperature;
    unsigned long frequency;
} CPUStats;

typedef struct {
    double usage;
    unsigned long long memory_total;
    unsigned long long memory_used;
    double temperature;
    double power;
    unsigned long clock;
    unsigned long memory_clock;
    double fan_rpm;
    double engine_usage;
    char name[128];
} GPUInfo;

typedef struct {
    unsigned long long read_bytes;
    unsigned long long write_bytes;
    double read_rate;
    double write_rate;
} DiskInfo;

typedef struct {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    double rx_rate;
    double tx_rate;
} NetworkInfo;

typedef struct {
    int present;
    double percentage;
    double power_watts;
    double temperature;
    char status[64];
    char name[64];
} BatteryInfo;

typedef struct {
    int storage_temperature_available;
    double storage_temperature;
    char storage_name[64];
    int fan_available;
    double fan_rpm;
    char fan_name[64];
} SensorInfo;

typedef struct {
    int pid;
    char name[256];
    char state;
    unsigned long utime;
    unsigned long stime;
    long rss;
    double cpu_usage;
    double mem_usage;
    char command_line[512];
} ProcessInfo;

typedef struct {
    double cpu_usage[HISTORY_SIZE];
    double memory_usage[HISTORY_SIZE];
    double gpu_usage[HISTORY_SIZE];
    double gpu_memory[HISTORY_SIZE];
    double gpu_temperature[HISTORY_SIZE];
    double disk_read[HISTORY_SIZE];
    double disk_write[HISTORY_SIZE];
    double network_rx[HISTORY_SIZE];
    double network_tx[HISTORY_SIZE];
    double battery[HISTORY_SIZE];
    long timestamps[HISTORY_SIZE];
    int index;
    int count;
} HistoryData;

#endif
