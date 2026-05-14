#include "metrics_sampler.h"

#include <string.h>

#include "proc_parser.h"

void collect_metrics_snapshot(MetricsSnapshot *snapshot) {
    if (!snapshot) {
        return;
    }

    snapshot->cpu_ok = read_cpu_stats(&snapshot->cpu, snapshot->cores, &snapshot->cores_count) == 0;
    snapshot->memory_ok = read_memory_info(&snapshot->memory) == 0;
    snapshot->gpu_ok = read_gpu_info_for_source(&snapshot->gpu, snapshot->gpu_source) == 0;
    if (snapshot->gpu_ok && snapshot->gpu.memory_total > 0) {
        snapshot->gpu_memory_percent =
            (double)snapshot->gpu.memory_used / (double)snapshot->gpu.memory_total * 100.0;
    }
    snapshot->disk_ok = read_disk_info(&snapshot->disk) == 0;
    snapshot->network_ok = read_network_info(&snapshot->network) == 0;
    snapshot->battery_ok = read_battery_info(&snapshot->battery) == 0;
    snapshot->sensor_ok = read_sensor_info(&snapshot->sensor) == 0;
    snapshot->processes_ok = get_processes(snapshot->processes, &snapshot->process_count) == 0;
    if (snapshot->process_count > MAX_PROCESSES) {
        snapshot->process_count = MAX_PROCESSES;
    }
}
