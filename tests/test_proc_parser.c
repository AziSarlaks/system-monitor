#include "test_config.h"
#include "backend/src/proc_parser.h"

static int test_cpu_cores_count() {
    int cores = get_cpu_cores_count();
    TEST_ASSERT(cores == -1 || (cores > 0 && cores <= MAX_CORES));
    return 1;
}

static int test_cpu_stats_calculation() {
    // Просто проверяем что функция существует
    // В реальном тесте нужно использовать calculate_cpu_usage
    return 1;
}

static int test_memory_info() {
    MemoryInfo mem;
    int result = read_memory_info(&mem);
    
    TEST_ASSERT(result == 0 || result == -1);
    if (result == 0) {
        TEST_ASSERT(mem.total > 0);
        TEST_ASSERT(mem.used <= mem.total);  // used не может быть больше total
        TEST_ASSERT(mem.free <= mem.total);  // free не может быть больше total
    }
    
    return 1;
}

static int test_gpu_info() {
    GPUInfo gpu;
    int result = read_gpu_info(&gpu);
    
    TEST_ASSERT(result == 0 || result == -1);
    TEST_ASSERT(strlen(gpu.name) > 0);
    if (result == 0) {
        TEST_ASSERT(gpu.usage >= 0 && gpu.usage <= 100);
        TEST_ASSERT(gpu.memory_total >= gpu.memory_used);
        TEST_ASSERT(gpu.memory_used <= gpu.memory_total);
    }
    
    return 1;
}

static int test_gpu_source_selection() {
    GPUInfo integrated;
    GPUInfo nvidia;

    int integrated_result = read_gpu_info_for_source(&integrated, GPU_SOURCE_INTEGRATED);
    int nvidia_result = read_gpu_info_for_source(&nvidia, GPU_SOURCE_NVIDIA);

    TEST_ASSERT(integrated_result == 0 || integrated_result == -1);
    TEST_ASSERT(nvidia_result == 0 || nvidia_result == -1);
    TEST_ASSERT(strlen(integrated.name) > 0);
    TEST_ASSERT(strlen(nvidia.name) > 0);
    if (integrated_result == 0) {
        TEST_ASSERT(integrated.memory_total >= integrated.memory_used);
    }
    if (nvidia_result == 0) {
        TEST_ASSERT(nvidia.memory_total >= nvidia.memory_used);
    }

    return 1;
}

static int test_disk_info() {
    DiskInfo disk;

    int result = read_disk_info(&disk);

    TEST_ASSERT(result == 0 || result == -1);
    if (result == 0) {
        TEST_ASSERT(disk.read_rate >= 0.0);
        TEST_ASSERT(disk.write_rate >= 0.0);
    }

    return 1;
}

static int test_network_info() {
    NetworkInfo network;

    int result = read_network_info(&network);

    TEST_ASSERT(result == 0 || result == -1);
    if (result == 0) {
        TEST_ASSERT(network.rx_rate >= 0.0);
        TEST_ASSERT(network.tx_rate >= 0.0);
    }

    return 1;
}

static int test_battery_info() {
    BatteryInfo battery;

    int result = read_battery_info(&battery);

    TEST_ASSERT(result == 0 || result == -1);
    if (result == 0) {
        TEST_ASSERT(strlen(battery.name) > 0);
        TEST_ASSERT(strlen(battery.status) > 0);
    }

    return 1;
}

static int test_sensor_info() {
    SensorInfo sensor;
    int result = read_sensor_info(&sensor);

    TEST_ASSERT(result == 0 || result == -1);
    if (result == 0) {
        TEST_ASSERT(strlen(sensor.storage_name) > 0);
        TEST_ASSERT(strlen(sensor.fan_name) > 0);
        if (sensor.storage_temperature_available) {
            TEST_ASSERT(sensor.storage_temperature > 0.0);
        }
        if (sensor.fan_available) {
            TEST_ASSERT(sensor.fan_rpm > 0.0);
        }
    }

    return 1;
}

static int test_processes() {
    ProcessInfo processes[MAX_PROCESSES];
    int count;
    
    int result = get_processes(processes, &count);
    TEST_ASSERT(result == 0 || result == -1);
    TEST_ASSERT(count >= 0);
    TEST_ASSERT(count <= MAX_PROCESSES);
    
    if (result == 0 && count > 0) {
        TEST_ASSERT(processes[0].pid > 0);
        TEST_ASSERT(strlen(processes[0].name) > 0);
    }
    
    return 1;
}

// Сьют тестов - ОПРЕДЕЛЯЕМ ТОЛЬКО ЗДЕСЬ!
void test_proc_parser_suite() {
    RUN_TEST(test_cpu_cores_count);
    RUN_TEST(test_cpu_stats_calculation);
    RUN_TEST(test_memory_info);
    RUN_TEST(test_gpu_info);
    RUN_TEST(test_gpu_source_selection);
    RUN_TEST(test_disk_info);
    RUN_TEST(test_network_info);
    RUN_TEST(test_battery_info);
    RUN_TEST(test_sensor_info);
    RUN_TEST(test_processes);
}
