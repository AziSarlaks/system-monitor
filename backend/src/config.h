#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "2.0.0"
#define VERSION_DATE "2026-02-16"

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_PROCESSES 512
#define UPDATE_INTERVAL_MS 2000
#define MAX_CONNECTIONS 10
#define MAX_CORES 32
#define HISTORY_SIZE 60

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
    char name[128];
} GPUInfo;

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
    long timestamps[HISTORY_SIZE];
    int index;
    int count;
} HistoryData;

#endif