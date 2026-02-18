#include "test_config.h"
#include "../backend/src/json_formatter.h"
#include "../backend/src/config.h"

// Моковые данные
static void mock_cpu_stats(CPUStats *cpu) {
    cpu->usage_percent = 45.5;
    cpu->temperature = 52.3;
    cpu->frequency = 2400;
}

static void mock_cores(CPUStats *cores, int count) {
    for (int i = 0; i < count; i++) {
        cores[i].usage_percent = 20.0 + i * 5.0;
        cores[i].temperature = 45.0 + i * 2.0;
        cores[i].frequency = 2400 + i * 100;
    }
}

static void mock_memory(MemoryInfo *mem) {
    mem->total = 16ULL * 1024 * 1024 * 1024;
    mem->used = 8ULL * 1024 * 1024 * 1024;
    mem->free = 6ULL * 1024 * 1024 * 1024;
    mem->cached = 2ULL * 1024 * 1024 * 1024;
    mem->percentage = 50.0;
}

static void mock_gpu(GPUInfo *gpu) {
    gpu->usage = 45.5;
    gpu->memory_total = 8ULL * 1024 * 1024 * 1024;
    gpu->memory_used = 4ULL * 1024 * 1024 * 1024;
    gpu->temperature = 65.0;
    gpu->power = 120.5;
    gpu->clock = 1800;
    strcpy(gpu->name, "Test GPU");
}

static void mock_processes(ProcessInfo *processes, int count) {
    for (int i = 0; i < count; i++) {
        processes[i].pid = 1000 + i;
        sprintf(processes[i].name, "test%d", i);
        processes[i].state = 'R';
        processes[i].rss = 1024 * (i + 1);
        processes[i].cpu_usage = 5.0 * (i + 1);
        sprintf(processes[i].command_line, "/bin/test%d --option", i);
    }
}

static int test_json_basic_structure() {
    char buffer[8192];
    
    CPUStats cpu;
    CPUStats cores[4];
    MemoryInfo mem;
    GPUInfo gpu;
    ProcessInfo processes[2];
    
    mock_cpu_stats(&cpu);
    mock_cores(cores, 4);
    mock_memory(&mem);
    mock_gpu(&gpu);
    mock_processes(processes, 2);
    
    format_system_info_json(buffer, sizeof(buffer), 
                           &cpu, cores, 4,
                           &mem, &gpu, processes, 2);
    
    TEST_ASSERT(strstr(buffer, "timestamp") != NULL);
    TEST_ASSERT(strstr(buffer, "cpu") != NULL);
    TEST_ASSERT(strstr(buffer, "memory") != NULL);
    TEST_ASSERT(strstr(buffer, "gpu") != NULL);
    TEST_ASSERT(strstr(buffer, "processes") != NULL);
    
    return 1;
}

// Сьют тестов
void test_json_formatter_suite() {
    RUN_TEST(test_json_basic_structure);
}