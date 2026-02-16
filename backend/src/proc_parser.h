#ifndef PROC_PARSER_H
#define PROC_PARSER_H

#include "config.h"

int get_cpu_cores_count();
int read_cpu_stats(CPUStats *cpu, CPUStats *cores, int *cores_count);
int read_memory_info(MemoryInfo *mem);
int read_gpu_info(GPUInfo *gpu);
int get_processes(ProcessInfo *processes, int *count);

#endif