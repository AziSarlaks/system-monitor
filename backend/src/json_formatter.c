#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include "config.h"
#include "json_formatter.h"

static void json_sanitize_string(const char *input, char *output, int max_len) {
    if (!input || !output || max_len <= 0) {
        if (output && max_len > 0) output[0] = '\0';
        return;
    }
    
    int i = 0, j = 0;
    
    while (input[i] && j < max_len - 1) {
        unsigned char c = (unsigned char)input[i];
        
        if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
            output[j++] = c;
        }
        else if (c == '"' || c == '\\') {
            if (j + 2 < max_len) {
                output[j++] = '\\';
                output[j++] = c;
            }
        }
        else if (c == '\n' || c == '\r' || c == '\t') {
            if (j + 2 < max_len) {
                output[j++] = '\\';
                if (c == '\n') output[j++] = 'n';
                else if (c == '\r') output[j++] = 'r';
                else if (c == '\t') output[j++] = 't';
            }
        }
        else if (c > 127) {
            output[j++] = c;
        }
        i++;
    }
    output[j] = '\0';
    
    if (j == 0 && max_len > 10) {
        strcpy(output, "[unknown]");
    }
}

static int safe_snprintf(char *buffer, int buffer_size, int offset, const char *format, ...) {
    if (offset >= buffer_size - 1) {
        return 0;
    }
    
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer + offset, buffer_size - offset, format, args);
    va_end(args);
    
    if (written < 0 || written >= buffer_size - offset) {
        return 0;
    }
    
    return written;
}

void format_system_info_json(char *buffer, int buffer_size, 
                            CPUStats *cpu, CPUStats *cores, int cores_count,
                            MemoryInfo *mem,
                            GPUInfo *gpu,
                            ProcessInfo *processes, int process_count) {
    if (buffer_size < 1024) {
        snprintf(buffer, buffer_size, "{\"error\":\"buffer too small\"}");
        return;
    }
    
    buffer[0] = '\0';
    int offset = 0;
    time_t now = time(NULL);
    
    if (gpu->memory_total > 100ULL * 1024 * 1024 * 1024) { // Больше 100GB - явно ошибка
        printf("⚠️ GPU memory_total слишком большой: %llu, исправляем\n", gpu->memory_total);
        gpu->memory_total = 8ULL * 1024 * 1024 * 1024; // 8GB
    }
    
    if (gpu->memory_used > gpu->memory_total) {
        printf("⚠️ GPU memory_used больше memory_total, исправляем\n");
        gpu->memory_used = gpu->memory_total * gpu->usage / 100.0;
    }
    
    offset += safe_snprintf(buffer, buffer_size, offset,
        "{\n"
        "  \"timestamp\": %ld,\n"
        "  \"cpu\": {\n"
        "    \"usage\": %.1f,\n"
        "    \"cores_count\": %d,\n"
        "    \"temperature\": %.1f,\n"
        "    \"frequency\": %lu,\n"
        "    \"cores\": [",
        now,
        cpu->usage_percent,
        cores_count,
        cpu->temperature,
        cpu->frequency);
    
    if (offset == 0) {
        snprintf(buffer, buffer_size, "{\"error\":\"buffer overflow at start\"}");
        return;
    }
    
    int actual_cores = (cores_count < MAX_CORES) ? cores_count : MAX_CORES;
    for (int i = 0; i < actual_cores; i++) {
        double core_usage = cores[i].usage_percent;
        if (core_usage > 100) core_usage = 100;
        if (core_usage < 0) core_usage = 0;
        
        if (i > 0) {
            if (offset < buffer_size - 2) {
                buffer[offset++] = ',';
                buffer[offset] = '\0';
            }
        }
        
        int written = safe_snprintf(buffer, buffer_size, offset,
            "\n      {\"core\": %d, \"usage\": %.1f}",
            i, core_usage);
        
        if (written > 0) {
            offset += written;
        }
        
        if (offset >= buffer_size - 200) {
            break;
        }
    }
    
    int written = safe_snprintf(buffer, buffer_size, offset,
        "\n    ]\n"
        "  },\n"
        "  \"memory\": {\n"
        "    \"total\": %llu,\n"
        "    \"used\": %llu,\n"
        "    \"free\": %llu,\n"
        "    \"cached\": %llu,\n"
        "    \"percentage\": %.1f\n"
        "  },\n"
        "  \"gpu\": {\n"
        "    \"usage\": %.1f,\n"
        "    \"memory_total\": %llu,\n"
        "    \"memory_used\": %llu,\n"
        "    \"temperature\": %.1f,\n"
        "    \"power\": %.1f,\n"
        "    \"clock\": %lu,\n"
        "    \"name\": \"%s\"\n"
        "  },\n"
        "  \"processes\": [",
        mem->total, mem->used, mem->free, mem->cached, mem->percentage,
        gpu->usage, gpu->memory_total, gpu->memory_used,
        gpu->temperature, gpu->power, gpu->clock, gpu->name);
    
    if (written > 0) {
        offset += written;
    } else {
        if (offset < buffer_size - 10) {
            strcpy(buffer + offset, "]\n}");
        }
        return;
    }
    
    int limit = (process_count > 10) ? 10 : process_count;
    int processes_added = 0;
    
    for (int i = 0; i < limit; i++) {
        ProcessInfo *p = &processes[i];
        
        if (buffer_size - offset < 500) {
            break;
        }
        
        char safe_cmd[512];
        char safe_name[256];
        
        json_sanitize_string(p->command_line, safe_cmd, sizeof(safe_cmd));
        json_sanitize_string(p->name, safe_name, sizeof(safe_name));
        
        if (strlen(safe_name) == 0) {
            strcpy(safe_name, "unknown");
        }
        if (strlen(safe_cmd) == 0) {
            strcpy(safe_cmd, safe_name);
        }
        
        if (processes_added > 0) {
            if (offset < buffer_size - 2) {
                buffer[offset++] = ',';
                buffer[offset] = '\0';
            }
        }
        
        written = safe_snprintf(buffer, buffer_size, offset,
            "\n    {\n"
            "      \"pid\": %d,\n"
            "      \"name\": \"%s\",\n"
            "      \"state\": \"%c\",\n"
            "      \"memory\": %lu,\n"
            "      \"cpu\": %.1f,\n"
            "      \"command\": \"%s\"\n"
            "    }",
            p->pid, safe_name, p->state, p->rss * 1024, p->cpu_usage, safe_cmd);
        
        if (written > 0) {
            offset += written;
            processes_added++;
        }
        
        if (offset >= buffer_size - 100) {
            break;
        }
    }
    
    if (buffer_size - offset >= 10) {
        strcpy(buffer + offset, "\n  ]\n}\n");
    } else {
        strncpy(buffer + buffer_size - 10, "\n]\n}\n", 10);
    }
    
    buffer[buffer_size - 1] = '\0';
}