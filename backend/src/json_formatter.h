#ifndef JSON_FORMATTER_H
#define JSON_FORMATTER_H

#include "config.h"

void format_system_info_json(char *buffer, int buffer_size, 
                            CPUStats *cpu, CPUStats *cores, int cores_count,
                            MemoryInfo *mem,
                            GPUInfo *gpu,
                            ProcessInfo *processes, int process_count);

#endif