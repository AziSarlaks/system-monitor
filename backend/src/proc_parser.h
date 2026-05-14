#ifndef PROC_PARSER_H
#define PROC_PARSER_H

#include "config.h"

int get_cpu_cores_count();
int read_cpu_stats(CPUStats *cpu, CPUStats *cores, int *cores_count);
int read_memory_info(MemoryInfo *mem);
int read_gpu_info(GPUInfo *gpu);
int read_gpu_info_for_source(GPUInfo *gpu, GPUSource source);
int read_disk_info(DiskInfo *disk);
int read_network_info(NetworkInfo *network);
int read_battery_info(BatteryInfo *battery);
int read_sensor_info(SensorInfo *sensor);
int get_processes(ProcessInfo *processes, int *count);

#endif
