#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <glob.h>
#include <time.h>
#include "config.h"
#include "proc_parser.h"

static int read_first_line(const char *path, char *buffer, size_t size) {
    FILE *fp;

    if (!path || !buffer || size == 0) {
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    if (!fgets(buffer, size, fp)) {
        fclose(fp);
        return -1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    fclose(fp);
    return 0;
}

static unsigned long read_ulong_file(const char *path) {
    char buffer[64];
    unsigned long value = 0;

    if (read_first_line(path, buffer, sizeof(buffer)) != 0) {
        return 0;
    }

    if (sscanf(buffer, "%lu", &value) != 1) {
        return 0;
    }

    return value;
}

static int read_first_glob_ulong(const char *pattern, unsigned long *value) {
    glob_t matches;
    int result = -1;

    if (!pattern || !value) {
        return -1;
    }

    if (glob(pattern, 0, NULL, &matches) != 0) {
        return -1;
    }

    for (size_t i = 0; i < matches.gl_pathc; i++) {
        unsigned long current = read_ulong_file(matches.gl_pathv[i]);
        if (current > 0) {
            *value = current;
            result = 0;
            break;
        }
    }

    globfree(&matches);
    return result;
}

static double read_milli_celsius_glob(const char *pattern) {
    unsigned long value = 0;

    if (read_first_glob_ulong(pattern, &value) != 0) {
        return 0.0;
    }

    return value / 1000.0;
}

static double read_gpu_hwmon_temperature(const char *card_name) {
    char pattern[256];

    snprintf(pattern,
             sizeof(pattern),
             "/sys/class/drm/%s/device/hwmon/hwmon*/temp1_input",
             card_name);
    return read_milli_celsius_glob(pattern);
}

static double read_gpu_hwmon_fan(const char *card_name) {
    char pattern[256];
    unsigned long value = 0;

    snprintf(pattern,
             sizeof(pattern),
             "/sys/class/drm/%s/device/hwmon/hwmon*/fan1_input",
             card_name);
    if (read_first_glob_ulong(pattern, &value) != 0) {
        return 0.0;
    }

    return (double)value;
}

static int parse_next_json_number(const char *start, double *value) {
    const char *colon;
    char *end;

    if (!start || !value) {
        return -1;
    }

    colon = strchr(start, ':');
    if (!colon) {
        return -1;
    }

    *value = strtod(colon + 1, &end);
    return end != colon + 1 ? 0 : -1;
}

static int read_intel_gpu_top_info(GPUInfo *gpu) {
    FILE *fp;
    char line[1024];
    double max_busy = 0.0;
    int found_busy = 0;

    if (!gpu) {
        return -1;
    }

    fp = popen("timeout 1s intel_gpu_top -J -s 250 -o - 2>/dev/null", "r");
    if (!fp) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        const char *cursor = line;
        while ((cursor = strstr(cursor, "\"busy\"")) != NULL) {
            double busy = 0.0;
            if (parse_next_json_number(cursor, &busy) == 0) {
                if (busy > max_busy) {
                    max_busy = busy;
                }
                found_busy = 1;
            }
            cursor += 6;
        }
    }

    pclose(fp);
    if (!found_busy) {
        return -1;
    }

    gpu->usage = max_busy > 100.0 ? 100.0 : max_busy;
    gpu->engine_usage = gpu->usage;
    return 0;
}

static const char *gpu_vendor_name(const char *vendor_id) {
    if (strcmp(vendor_id, "0x8086") == 0) return "Intel Integrated GPU";
    if (strcmp(vendor_id, "0x1002") == 0 || strcmp(vendor_id, "0x1022") == 0) return "AMD Integrated GPU";
    if (strcmp(vendor_id, "0x10de") == 0) return "NVIDIA GPU";
    return "GPU";
}

static int is_integrated_gpu_vendor(const char *vendor_id) {
    return strcmp(vendor_id, "0x8086") == 0 ||
           strcmp(vendor_id, "0x1002") == 0 ||
           strcmp(vendor_id, "0x1022") == 0;
}

static int read_drm_gpu_info(GPUInfo *gpu, GPUSource source) {
    DIR *dir = opendir("/sys/class/drm");
    struct dirent *entry;

    if (!dir) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char vendor_path[256];
        char device_path[256];
        char freq_path[256];
        char card_name[32];
        char vendor_id[32];
        char device_id[32] = "";

        if (strncmp(entry->d_name, "card", 4) != 0 || !isdigit(entry->d_name[4]) ||
            strchr(entry->d_name, '-') != NULL) {
            continue;
        }

        snprintf(card_name, sizeof(card_name), "%.31s", entry->d_name);
        snprintf(vendor_path, sizeof(vendor_path), "/sys/class/drm/%s/device/vendor", card_name);
        if (read_first_line(vendor_path, vendor_id, sizeof(vendor_id)) != 0) {
            continue;
        }

        if (source == GPU_SOURCE_INTEGRATED && !is_integrated_gpu_vendor(vendor_id)) {
            continue;
        }

        if (source == GPU_SOURCE_NVIDIA && strcmp(vendor_id, "0x10de") != 0) {
            continue;
        }

        snprintf(device_path, sizeof(device_path), "/sys/class/drm/%s/device/device", card_name);
        read_first_line(device_path, device_id, sizeof(device_id));

        memset(gpu, 0, sizeof(GPUInfo));
        snprintf(gpu->name, sizeof(gpu->name), "%s", gpu_vendor_name(vendor_id));
        gpu->temperature = read_gpu_hwmon_temperature(card_name);
        gpu->fan_rpm = read_gpu_hwmon_fan(card_name);

        if (source == GPU_SOURCE_INTEGRATED) {
            snprintf(freq_path, sizeof(freq_path),
                     "/sys/class/drm/%s/gt/gt0/rps_cur_freq_mhz", card_name);
            gpu->clock = read_ulong_file(freq_path);
            if (gpu->clock == 0) {
                snprintf(freq_path, sizeof(freq_path),
                         "/sys/class/drm/%s/gt/gt1/rps_cur_freq_mhz", card_name);
                gpu->clock = read_ulong_file(freq_path);
            }
            if (gpu->clock == 0) {
                snprintf(freq_path, sizeof(freq_path),
                         "/sys/class/drm/%s/gt_cur_freq_mhz", card_name);
                gpu->clock = read_ulong_file(freq_path);
            }
            if (strcmp(vendor_id, "0x8086") == 0) {
                read_intel_gpu_top_info(gpu);
            }
        }

        closedir(dir);
        return 0;
    }

    closedir(dir);
    return -1;
}

double get_cpu_temperature() {
    double temp = 0.0;
    
    // 1. Пробуем чтение из thermal zones
    const char *thermal_paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
        "/sys/class/thermal/thermal_zone2/temp",
        "/sys/class/thermal/thermal_zone3/temp"
    };
    
    for (int i = 0; i < 4; i++) {
        FILE *fp = fopen(thermal_paths[i], "r");
        if (fp) {
            int temp_raw;
            if (fscanf(fp, "%d", &temp_raw) == 1) {
                // Обычно температура в миллиградусах Цельсия (м°C)
                temp = temp_raw / 1000.0;
                fclose(fp);
                
                // Проверка на реалистичность (10°C - 110°C)
                if (temp >= 10.0 && temp <= 110.0) {
                    return temp;
                }
            }
            fclose(fp);
        }
    }
    
    // 2. Пробуем чтение из hwmon
    for (int i = 0; i < 10; i++) {
        char path[256];
        snprintf(path, sizeof(path), 
                 "/sys/class/hwmon/hwmon%d/temp1_input", i);
        
        FILE *fp = fopen(path, "r");
        if (fp) {
            int temp_raw;
            if (fscanf(fp, "%d", &temp_raw) == 1) {
                temp = temp_raw / 1000.0;
                fclose(fp);
                
                if (temp >= 10.0 && temp <= 110.0) {
                    return temp;
                }
            }
            fclose(fp);
        }
    }
    
    // 3. Пробуем через команду sensors (требует установки lm-sensors)
    FILE *fp = popen("sensors | grep -i 'core\\|cpu' | grep -oP '\\+\\d+\\.\\d+°C' | head -1 | tr -d '+°C'", "r");
    if (fp) {
        if (fscanf(fp, "%lf", &temp) == 1) {
            pclose(fp);
            if (temp >= 10.0 && temp <= 110.0) {
                return temp;
            }
        }
        pclose(fp);
    }
    
    return 0.0;
}

unsigned long get_cpu_frequency() {
    unsigned long freq = 0;
    
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (fp) {
        if (fscanf(fp, "%lu", &freq) == 1) {
            freq = freq / 1000;
            fclose(fp);
            return freq;
        }
        fclose(fp);
    }
    
    fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "cpu MHz")) {
                double mhz;
                sscanf(line, "cpu MHz : %lf", &mhz);
                freq = (unsigned long)mhz;
                fclose(fp);
                return freq;
            }
        }
        fclose(fp);
    }
    
    fp = popen("lscpu | grep 'CPU MHz' | grep -oP '\\d+\\.\\d+' | head -1", "r");
    if (fp) {
        double mhz;
        if (fscanf(fp, "%lf", &mhz) == 1) {
            freq = (unsigned long)mhz;
            pclose(fp);
            return freq;
        }
        pclose(fp);
    }
    
    return 0;
}

int get_cpu_cores_count() {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return -1;
    
    char line[256];
    int cores = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "processor") == line) {
            cores++;
        }
    }
    
    fclose(fp);
    return cores > 0 ? cores : -1;
}

int read_cpu_stats(CPUStats *cpu, CPUStats *cores, int *cores_count) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        if (cores_count) {
            *cores_count = 0;
        }
        return -1;
    }
    
    double temp = get_cpu_temperature();

    char line[256];
    *cores_count = 0;
    int total_cores_found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) == 0) {
            sscanf(line + 5, 
                   "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                   &cpu->user, &cpu->nice, &cpu->system, &cpu->idle,
                   &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->steal,
                   &cpu->guest, &cpu->guest_nice);
            
            cpu->total = cpu->user + cpu->nice + cpu->system + cpu->idle +
                        cpu->iowait + cpu->irq + cpu->softirq + cpu->steal;
            cpu->usage_percent = 0.0;
            
            // Добавляем температуру и частоту для общего CPU
            cpu->temperature = temp;
            cpu->frequency = get_cpu_frequency();
        }
        else if (strncmp(line, "cpu", 3) == 0 && isdigit(line[3])) {
            if (total_cores_found < MAX_CORES) {
                sscanf(line + 3, "%*d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                       &cores[total_cores_found].user, &cores[total_cores_found].nice,
                       &cores[total_cores_found].system, &cores[total_cores_found].idle,
                       &cores[total_cores_found].iowait, &cores[total_cores_found].irq,
                       &cores[total_cores_found].softirq, &cores[total_cores_found].steal,
                       &cores[total_cores_found].guest, &cores[total_cores_found].guest_nice);
                
                cores[total_cores_found].total = 
                    cores[total_cores_found].user + cores[total_cores_found].nice + 
                    cores[total_cores_found].system + cores[total_cores_found].idle +
                    cores[total_cores_found].iowait + cores[total_cores_found].irq +
                    cores[total_cores_found].softirq + cores[total_cores_found].steal;
                cores[total_cores_found].usage_percent = 0.0;
                cores[total_cores_found].temperature = cpu->temperature;
                cores[total_cores_found].frequency = cpu->frequency;
                
                total_cores_found++;
            }
        }
    }
    
    fclose(fp);
    *cores_count = total_cores_found;
    
    if (*cores_count == 0) {
        return -1;
    }
    
    return 0;
}

int read_memory_info(MemoryInfo *mem) {
    memset(mem, 0, sizeof(MemoryInfo));
    
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }
    
    char line[128];
    unsigned long long total = 0, free = 0, available = 0, buffers = 0, cached = 0, sreclaimable = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "MemTotal:")) {
            sscanf(line, "MemTotal: %llu kB", &total);
        } else if (strstr(line, "MemFree:")) {
            sscanf(line, "MemFree: %llu kB", &free);
        } else if (strstr(line, "MemAvailable:")) {
            sscanf(line, "MemAvailable: %llu kB", &available);
        } else if (strstr(line, "Buffers:")) {
            sscanf(line, "Buffers: %llu kB", &buffers);
        } else if (strstr(line, "Cached:")) {
            sscanf(line, "Cached: %llu kB", &cached);
        } else if (strstr(line, "SReclaimable:")) {
            sscanf(line, "SReclaimable: %llu kB", &sreclaimable);
        }
    }
    fclose(fp);
    
    mem->total = total * 1024;
    mem->free = free * 1024;
    
    mem->cached = (cached + sreclaimable) * 1024;
    
    if (available > 0) {
        mem->used = mem->total - (available * 1024);
    } else {
        unsigned long long reclaimable_kb = free + buffers + cached + sreclaimable;
        unsigned long long used_kb = total > reclaimable_kb ? total - reclaimable_kb : 0;
        mem->used = used_kb * 1024;
    }
    
    if (mem->used > mem->total) {
        mem->used = mem->total;
    }
    
    if (mem->total > 0) {
        mem->percentage = (double)mem->used / mem->total * 100.0;
    } else {
        return -1;
    }
    
    return 0;
}

static int read_nvidia_smi_gpu_info(GPUInfo *gpu) {
    memset(gpu, 0, sizeof(GPUInfo));
    strcpy(gpu->name, "NVIDIA GPU unavailable");
    
    FILE *fp = popen("nvidia-smi --query-gpu=utilization.gpu,memory.total,memory.used,temperature.gpu,power.draw,clocks.current.graphics,name --format=csv,noheader,nounits 2>/dev/null", "r");
    
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            char *line_ptr = line;
            while (*line_ptr == ' ' || *line_ptr == '\t' || *line_ptr == '\n' || *line_ptr == '\r') {
                line_ptr++;
            }
            
            char *parts[10];
            int part_count = 0;
            char *token = strtok(line_ptr, ",");
            
            while (token && part_count < 10) {
                while (*token == ' ' || *token == '\t') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
                    *end = '\0';
                    end--;
                }
                
                parts[part_count++] = token;
                token = strtok(NULL, ",");
            }
            
            if (part_count >= 7) {
                double usage = 0, temp = 0, power = 0;
                unsigned long long mem_total = 0, mem_used = 0;
                unsigned long clock = 0;
                char name[128] = "";
                
                if (sscanf(parts[0], "%lf", &usage) != 1 ||
                    sscanf(parts[1], "%llu", &mem_total) != 1 ||
                    sscanf(parts[2], "%llu", &mem_used) != 1 ||
                    sscanf(parts[3], "%lf", &temp) != 1 ||
                    sscanf(parts[4], "%lf", &power) != 1 ||
                    sscanf(parts[5], "%lu", &clock) != 1) {
                    pclose(fp);
                    return -1;
                }
                
                strncpy(name, parts[6], sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0';

                gpu->usage = usage;
                gpu->memory_total = mem_total * 1024 * 1024;
                gpu->memory_used = mem_used * 1024 * 1024;
                gpu->temperature = temp;
                gpu->power = power;
                gpu->clock = clock;
                strncpy(gpu->name, name, sizeof(gpu->name) - 1);
                gpu->name[sizeof(gpu->name) - 1] = '\0';
                
                pclose(fp);
                return 0;
            }
        }
        pclose(fp);
    }

    return -1;
}

int read_gpu_info_for_source(GPUInfo *gpu, GPUSource source) {
    if (!gpu) {
        return -1;
    }

    if (source == GPU_SOURCE_INTEGRATED) {
        memset(gpu, 0, sizeof(GPUInfo));
        strcpy(gpu->name, "Integrated GPU unavailable");
        return read_drm_gpu_info(gpu, GPU_SOURCE_INTEGRATED);
    }

    if (read_nvidia_smi_gpu_info(gpu) == 0) {
        return 0;
    }
    memset(gpu, 0, sizeof(GPUInfo));
    strcpy(gpu->name, "NVIDIA GPU unavailable");
    return read_drm_gpu_info(gpu, GPU_SOURCE_NVIDIA);
}

int read_gpu_info(GPUInfo *gpu) {
    return read_gpu_info_for_source(gpu, GPU_SOURCE_NVIDIA);
}

int read_disk_info(DiskInfo *disk) {
    static unsigned long long prev_read_bytes = 0;
    static unsigned long long prev_write_bytes = 0;
    static time_t prev_time = 0;
    unsigned long long read_sectors_total = 0;
    unsigned long long write_sectors_total = 0;
    time_t now = time(NULL);
    FILE *fp;

    if (!disk) {
        return -1;
    }

    memset(disk, 0, sizeof(DiskInfo));
    fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char name[64];
        unsigned long long reads_completed, reads_merged, read_sectors;
        unsigned long long read_ms, writes_completed, writes_merged, write_sectors;

        int parsed = sscanf(line,
                            "%*u %*u %63s %llu %llu %llu %llu %llu %llu %llu",
                            name,
                            &reads_completed,
                            &reads_merged,
                            &read_sectors,
                            &read_ms,
                            &writes_completed,
                            &writes_merged,
                            &write_sectors);
        (void)reads_completed;
        (void)reads_merged;
        (void)read_ms;
        (void)writes_completed;
        (void)writes_merged;

        if (parsed != 8) {
            continue;
        }

        if (strncmp(name, "loop", 4) == 0 || strncmp(name, "ram", 3) == 0) {
            continue;
        }

        int len = strlen(name);
        if (len > 0 && isdigit(name[len - 1]) &&
            (strncmp(name, "sd", 2) == 0 || strncmp(name, "vd", 2) == 0 || strncmp(name, "xvd", 3) == 0)) {
            continue;
        }
        if (strstr(name, "nvme") == name && strstr(name, "p") != NULL) {
            continue;
        }

        read_sectors_total += read_sectors;
        write_sectors_total += write_sectors;
    }

    fclose(fp);

    disk->read_bytes = read_sectors_total * 512ULL;
    disk->write_bytes = write_sectors_total * 512ULL;

    if (prev_time > 0 && now > prev_time) {
        double elapsed = difftime(now, prev_time);
        if (disk->read_bytes >= prev_read_bytes) {
            disk->read_rate = (double)(disk->read_bytes - prev_read_bytes) / elapsed;
        }
        if (disk->write_bytes >= prev_write_bytes) {
            disk->write_rate = (double)(disk->write_bytes - prev_write_bytes) / elapsed;
        }
    }

    prev_read_bytes = disk->read_bytes;
    prev_write_bytes = disk->write_bytes;
    prev_time = now;
    return 0;
}

int read_network_info(NetworkInfo *network) {
    static unsigned long long prev_rx_bytes = 0;
    static unsigned long long prev_tx_bytes = 0;
    static time_t prev_time = 0;
    unsigned long long rx_total = 0;
    unsigned long long tx_total = 0;
    time_t now = time(NULL);
    FILE *fp;

    if (!network) {
        return -1;
    }

    memset(network, 0, sizeof(NetworkInfo));
    fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char iface[64];
        unsigned long long rx_bytes, tx_bytes;
        unsigned long long skip[8];

        if (!strchr(line, ':')) {
            continue;
        }

        int parsed = sscanf(line,
                            " %63[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                            iface,
                            &rx_bytes,
                            &skip[0],
                            &skip[1],
                            &skip[2],
                            &skip[3],
                            &skip[4],
                            &skip[5],
                            &skip[6],
                            &tx_bytes);
        if (parsed != 10 || strcmp(iface, "lo") == 0) {
            continue;
        }

        rx_total += rx_bytes;
        tx_total += tx_bytes;
    }

    fclose(fp);

    network->rx_bytes = rx_total;
    network->tx_bytes = tx_total;

    if (prev_time > 0 && now > prev_time) {
        double elapsed = difftime(now, prev_time);
        if (network->rx_bytes >= prev_rx_bytes) {
            network->rx_rate = (double)(network->rx_bytes - prev_rx_bytes) / elapsed;
        }
        if (network->tx_bytes >= prev_tx_bytes) {
            network->tx_rate = (double)(network->tx_bytes - prev_tx_bytes) / elapsed;
        }
    }

    prev_rx_bytes = network->rx_bytes;
    prev_tx_bytes = network->tx_bytes;
    prev_time = now;
    return 0;
}

static double read_micro_unit_file(const char *path) {
    unsigned long value = read_ulong_file(path);
    return value > 0 ? value / 1000000.0 : 0.0;
}

static void make_power_supply_path(char *buffer, size_t size, const char *supply_name, const char *file_name) {
    snprintf(buffer, size, "/sys/class/power_supply/%.63s/%s", supply_name, file_name);
}

int read_battery_info(BatteryInfo *battery) {
    DIR *dir;
    struct dirent *entry;

    if (!battery) {
        return -1;
    }

    memset(battery, 0, sizeof(BatteryInfo));
    strcpy(battery->name, "No battery");
    strcpy(battery->status, "Unavailable");

    dir = opendir("/sys/class/power_supply");
    if (!dir) {
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char type_path[1024];
        char type[64];
        char path[1024];
        char supply_name[64];

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(supply_name, sizeof(supply_name), "%.63s", entry->d_name);
        make_power_supply_path(type_path, sizeof(type_path), supply_name, "type");
        if (read_first_line(type_path, type, sizeof(type)) != 0 || strcmp(type, "Battery") != 0) {
            continue;
        }

        battery->present = 1;
        snprintf(battery->name, sizeof(battery->name), "%s", supply_name);

        make_power_supply_path(path, sizeof(path), supply_name, "capacity");
        battery->percentage = read_ulong_file(path);

        make_power_supply_path(path, sizeof(path), supply_name, "status");
        if (read_first_line(path, battery->status, sizeof(battery->status)) != 0) {
            strcpy(battery->status, "Unknown");
        }

        make_power_supply_path(path, sizeof(path), supply_name, "power_now");
        battery->power_watts = read_micro_unit_file(path);
        if (battery->power_watts == 0.0) {
            double current;
            double voltage;
            make_power_supply_path(path, sizeof(path), supply_name, "current_now");
            current = read_micro_unit_file(path);
            make_power_supply_path(path, sizeof(path), supply_name, "voltage_now");
            voltage = read_micro_unit_file(path);
            battery->power_watts = current * voltage;
        }

        make_power_supply_path(path, sizeof(path), supply_name, "temp");
        battery->temperature = read_ulong_file(path) / 10.0;

        closedir(dir);
        return 0;
    }

    closedir(dir);
    return 0;
}

static int is_storage_hwmon_name(const char *name) {
    return name &&
           (strstr(name, "nvme") != NULL ||
            strstr(name, "drivetemp") != NULL ||
            strstr(name, "sata") != NULL ||
            strstr(name, "ata") != NULL);
}

int read_sensor_info(SensorInfo *sensor) {
    glob_t hwmons;

    if (!sensor) {
        return -1;
    }

    memset(sensor, 0, sizeof(SensorInfo));
    strcpy(sensor->storage_name, "Storage temperature unavailable");
    strcpy(sensor->fan_name, "Fan unavailable");

    if (glob("/sys/class/hwmon/hwmon*", 0, NULL, &hwmons) != 0) {
        return -1;
    }

    for (size_t i = 0; i < hwmons.gl_pathc; i++) {
        char name_path[512];
        char name[64] = "";
        char temp_path[512];
        char fan_path[512];
        unsigned long value = 0;

        snprintf(name_path, sizeof(name_path), "%s/name", hwmons.gl_pathv[i]);
        read_first_line(name_path, name, sizeof(name));

        if (!sensor->storage_temperature_available && is_storage_hwmon_name(name)) {
            for (int t = 1; t <= 8; t++) {
                snprintf(temp_path, sizeof(temp_path), "%s/temp%d_input", hwmons.gl_pathv[i], t);
                value = read_ulong_file(temp_path);
                if (value > 0) {
                    sensor->storage_temperature = value / 1000.0;
                    sensor->storage_temperature_available = 1;
                    snprintf(sensor->storage_name, sizeof(sensor->storage_name), "%s", name[0] ? name : "Storage");
                    break;
                }
            }
        }

        if (!sensor->fan_available) {
            for (int f = 1; f <= 8; f++) {
                snprintf(fan_path, sizeof(fan_path), "%s/fan%d_input", hwmons.gl_pathv[i], f);
                value = read_ulong_file(fan_path);
                if (value > 0) {
                    sensor->fan_rpm = (double)value;
                    sensor->fan_available = 1;
                    snprintf(sensor->fan_name, sizeof(sensor->fan_name), "%.48s fan%d", name[0] ? name : "System", f);
                    break;
                }
            }
        }
    }

    globfree(&hwmons);
    return 0;
}

int get_processes(ProcessInfo *processes, int *count) {
    DIR *dir = opendir("/proc");
    if (!dir) {
        if (count) {
            *count = 0;
        }
        return -1;
    }
    
    struct dirent *entry;
    *count = 0;
    
    static unsigned long long prev_total = 0;
    unsigned long long total = 0, idle = 0;
    
    FILE *stat_fp = fopen("/proc/stat", "r");
    if (stat_fp) {
        char line[256];
        if (fgets(line, sizeof(line), stat_fp)) {
            unsigned long long user, nice, system, idle_stat, iowait, irq, softirq, steal;
            sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle_stat, &iowait, &irq, &softirq, &steal);
            
            total = user + nice + system + idle_stat + iowait + irq + softirq + steal;
            idle = idle_stat + iowait;
        }
        fclose(stat_fp);
    }
    
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) ticks_per_sec = 100;
    
    while ((entry = readdir(dir)) != NULL && *count < MAX_PROCESSES) {
        int is_pid = 1;
        for (int i = 0; entry->d_name[i]; i++) {
            if (!isdigit(entry->d_name[i])) {
                is_pid = 0;
                break;
            }
        }
        
        if (!is_pid) continue;
        
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        char path[256];
        ProcessInfo *p = &processes[*count];
        p->pid = pid;
        
        strcpy(p->name, "unknown");
        p->state = '?';
        p->rss = 0;
        p->cpu_usage = 0.0;
        p->mem_usage = 0.0;
        strcpy(p->command_line, "");
        
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        FILE *fp = fopen(path, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "Name:", 5) == 0) {
                    char *name = line + 5;
                    while (*name == ' ' || *name == '\t') name++;
                    strncpy(p->name, name, 255);
                    p->name[strcspn(p->name, "\n")] = 0;
                } else if (strncmp(line, "State:", 6) == 0) {
                    p->state = line[7];
                } else if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%lu", &p->rss); // RSS в KB
                }
            }
            fclose(fp);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        fp = fopen(path, "r");
        if (fp) {
            char line[1024];
            if (fgets(line, sizeof(line), fp)) {
                unsigned long utime, stime;
                long rss_pages;
                char comm[256];
                
                sscanf(line, "%*d (%255[^)]) %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %ld",
                       comm, &utime, &stime, &rss_pages);
                
                if (strlen(comm) > 0 && strcmp(p->name, "unknown") == 0) {
                    strncpy(p->name, comm, sizeof(p->name) - 1);
                    p->name[sizeof(p->name) - 1] = '\0';
                }
                
                static unsigned long long prev_utime[MAX_PROCESSES] = {0};
                static unsigned long long prev_stime[MAX_PROCESSES] = {0};
                static int prev_pid[MAX_PROCESSES] = {0};
                
                unsigned long long prev_u = 0, prev_s = 0;
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (prev_pid[i] == pid) {
                        prev_u = prev_utime[i];
                        prev_s = prev_stime[i];
                        break;
                    }
                }
                
                if (prev_u > 0 || prev_s > 0) {
                    unsigned long long total_cpu_diff = total - prev_total;
                    if (total_cpu_diff > 0) {
                        unsigned long long proc_cpu_diff = (utime - prev_u) + (stime - prev_s);
                        p->cpu_usage = 100.0 * proc_cpu_diff / total_cpu_diff;
                        if (p->cpu_usage > 100.0) p->cpu_usage = 100.0;
                    }
                } else {
                    p->cpu_usage = 0.0;
                }
                
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (prev_pid[i] == pid || prev_pid[i] == 0) {
                        prev_pid[i] = pid;
                        prev_utime[i] = utime;
                        prev_stime[i] = stime;
                        break;
                    }
                }
                
                if (p->rss == 0 && rss_pages > 0) {
                    p->rss = rss_pages * sysconf(_SC_PAGESIZE) / 1024;
                }
            }
            fclose(fp);
        }
        
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        fp = fopen(path, "rb");
        if (fp) {
            int bytes = fread(p->command_line, 1, 511, fp);
            if (bytes > 0) {
                p->command_line[bytes] = '\0';
                for (int i = 0; i < bytes; i++) {
                    if (p->command_line[i] == '\0') {
                        p->command_line[i] = ' ';
                    }
                }
                int len = strlen(p->command_line);
                while (len > 0 && (p->command_line[len-1] == ' ' || 
                                   p->command_line[len-1] == '\n' || 
                                   p->command_line[len-1] == '\r')) {
                    p->command_line[len-1] = '\0';
                    len--;
                }
            }
            fclose(fp);
        }
        
        if (strlen(p->command_line) == 0) {
            strcpy(p->command_line, p->name);
        }
        
        struct sysinfo info;
        if (sysinfo(&info) == 0 && info.totalram > 0) {
            p->mem_usage = 100.0 * (p->rss * 1024) / (info.totalram * info.mem_unit);
        }
        
        (*count)++;
    }
    
    closedir(dir);
    
    prev_total = total;
    (void)idle;
    
    for (int i = 0; i < *count - 1; i++) {
        for (int j = i + 1; j < *count; j++) {
            if (processes[i].cpu_usage < processes[j].cpu_usage) {
                ProcessInfo temp = processes[i];
                processes[i] = processes[j];
                processes[j] = temp;
            }
        }
    }
    
    return 0;
}
