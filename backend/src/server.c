#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdarg.h>
#include "config.h"
#include "proc_parser.h"
#include "json_formatter.h"
#include "history.h"

static int server_socket = -1;
static pthread_t update_thread;
static volatile int running = 1;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

#define JSON_BUFFER_SIZE 65536
#define HISTORY_BUFFER_SIZE 16384

static char system_json[JSON_BUFFER_SIZE];
static char history_json[HISTORY_BUFFER_SIZE];

static CPUStats cpu_prev, cpu_curr;
static CPUStats cores_prev[MAX_CORES], cores_curr[MAX_CORES];
static GPUInfo gpu_info;
static HistoryData system_history;
static int cores_count = 0;

void calculate_cpu_usage(CPUStats *prev, CPUStats *curr) {
    if (!prev || !curr) return;
    
    double total_diff = curr->total - prev->total;
    double idle_diff = curr->idle - prev->idle;
    
    if (total_diff > 0) {
        curr->usage_percent = 100.0 * (1.0 - (idle_diff / total_diff));
        if (curr->usage_percent < 0) curr->usage_percent = 0;
        if (curr->usage_percent > 100) curr->usage_percent = 100;
    } else {
        curr->usage_percent = 0.0;
    }
}

void *update_data_thread(void *arg) {
    (void)arg;
    
    MemoryInfo mem;
    ProcessInfo processes[MAX_PROCESSES];
    int process_count = 0;
    
    srand(time(NULL));
    
    init_history(&system_history);
    
    if (read_cpu_stats(&cpu_prev, cores_prev, &cores_count) != 0) {
        cores_count = 4;
        cpu_prev.total = 1000;
        cpu_prev.idle = 800;
        for (int i = 0; i < cores_count; i++) {
            cores_prev[i].total = 250;
            cores_prev[i].idle = 200;
        }
    }
    
    read_gpu_info(&gpu_info);
    
    memcpy(&cpu_curr, &cpu_prev, sizeof(CPUStats));
    for (int i = 0; i < cores_count; i++) {
        memcpy(&cores_curr[i], &cores_prev[i], sizeof(CPUStats));
    }
    
    while (running) {
        usleep(UPDATE_INTERVAL_MS * 1000);
        
        read_cpu_stats(&cpu_curr, cores_curr, &cores_count);
        read_memory_info(&mem);
        read_gpu_info(&gpu_info);
        get_processes(processes, &process_count);
        
        calculate_cpu_usage(&cpu_prev, &cpu_curr);
        for (int i = 0; i < cores_count; i++) {
            calculate_cpu_usage(&cores_prev[i], &cores_curr[i]);
        }
        
        double gpu_memory_percent = 0.0;
        if (gpu_info.memory_total > 0) {
            gpu_memory_percent = (double)gpu_info.memory_used / gpu_info.memory_total * 100.0;
        }
        
        add_to_history(&system_history, 
                      cpu_curr.usage_percent,
                      mem.percentage,
                      gpu_info.usage,
                      gpu_memory_percent,
                      gpu_info.temperature);
        
        pthread_mutex_lock(&data_mutex);
        
        format_system_info_json(system_json, sizeof(system_json),
                               &cpu_curr, cores_curr, cores_count,
                               &mem, &gpu_info, processes, process_count);
        
        get_history_json(history_json, sizeof(history_json), &system_history);
        
        pthread_mutex_unlock(&data_mutex);
        
        memcpy(&cpu_prev, &cpu_curr, sizeof(CPUStats));
        for (int i = 0; i < cores_count; i++) {
            memcpy(&cores_prev[i], &cores_curr[i], sizeof(CPUStats));
        }
    }
    
    return NULL;
}

void send_http_response(int client_socket, int status, const char* content_type, const char* body) {
    char response[8192];
    const char* status_text;
    
    switch (status) {
        case 200: status_text = "OK"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown"; break;
    }
    
    int length = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Accept, Origin, User-Agent\r\n"
        "Access-Control-Expose-Headers: Content-Length, Content-Type\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Vary: Origin\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status,
        status_text,
        content_type,
        (long)strlen(body),
        body);
    
    if (length >= (int)sizeof(response) - 1) {
        response[sizeof(response) - 1] = '\0';
        length = sizeof(response) - 1;
    }
    
    send(client_socket, response, length, 0);
}

void handle_client(int client_socket) {
    char request[4096];
    char method[16], path[256], protocol[16];
    
    int bytes_read = recv(client_socket, request, sizeof(request) - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    request[bytes_read] = '\0';
    
    if (sscanf(request, "%s %s %s", method, path, protocol) != 3) {
        printf("Invalid request format\n");
        close(client_socket);
        return;
    }
    
    printf("Request: %s %s %s\n", method, path, protocol);
    
    if (strcmp(method, "OPTIONS") == 0) {
        printf("Processing CORS preflight request\n");
        
        char response[1024];
        int length = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Accept, Origin, User-Agent\r\n"
            "Access-Control-Expose-Headers: Content-Length, Content-Type\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Vary: Origin\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n");
        
        if (length > 0) {
            send(client_socket, response, length, 0);
        }
        close(client_socket);
        return;
    }
    
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            const char* html = 
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "    <title>System Monitor Server</title>\n"
                "    <style>\n"
                "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
                "        .container { max-width: 800px; margin: 0 auto; }\n"
                "        .status { padding: 10px; background: #f0f0f0; border-radius: 5px; }\n"
                "        .online { color: green; }\n"
                "        .data { background: #f9f9f9; padding: 20px; margin-top: 20px; border-radius: 5px; }\n"
                "        code { background: #eee; padding: 2px 4px; border-radius: 3px; }\n"
                "        a { color: #0066cc; text-decoration: none; }\n"
                "        a:hover { text-decoration: underline; }\n"
                "    </style>\n"
                "</head>\n"
                "<body>\n"
                "    <div class=\"container\">\n"
                "        <h1>System Monitor Server</h1>\n"
                "        <div class=\"status\">\n"
                "            <p class=\"online\">✅ Server is running!</p>\n"
                "            <p><strong>API Endpoints:</strong></p>\n"
                "            <ul>\n"
                "                <li><a href=\"/api/system\">GET /api/system</a> - System information (JSON)</li>\n"
                "                <li><a href=\"/api/history\">GET /api/history</a> - System history (JSON)</li>\n"
                "                <li><a href=\"/api/health\">GET /api/health</a> - Health check (JSON)</li>\n"
                "            </ul>\n"
                "            <p><strong>Frontend:</strong> Open <code>frontend/index.html</code> in your browser</p>\n"
                "        </div>\n"
                "        <div class=\"data\">\n"
                "            <h3>Server Information</h3>\n"
                "            <p><strong>URL:</strong> <code>http://localhost:8080</code></p>\n"
                "            <p><strong>CORS:</strong> Enabled (all origins allowed)</p>\n"
                "            <p><strong>Update Interval:</strong> 2 seconds</p>\n"
                "        </div>\n"
                "    </div>\n"
                "</body>\n"
                "</html>";
            
            send_http_response(client_socket, 200, "text/html; charset=utf-8", html);
            
        } else if (strcmp(path, "/api/system") == 0) {
            printf("Serving system data\n");
            pthread_mutex_lock(&data_mutex);
            
            if (strlen(system_json) == 0) {
                const char* error_json = "{\"error\":\"Data not ready yet\",\"timestamp\":0}";
                send_http_response(client_socket, 200, "application/json", error_json);
            } else {
                send_http_response(client_socket, 200, "application/json", system_json);
            }
            
            pthread_mutex_unlock(&data_mutex);
            
        } else if (strcmp(path, "/api/history") == 0) {
            printf("Serving history data\n");
            pthread_mutex_lock(&data_mutex);
            
            if (strlen(history_json) == 0) {
                const char* error_json = "{\"error\":\"History not ready yet\",\"timestamp\":0}";
                send_http_response(client_socket, 200, "application/json", error_json);
            } else {
                send_http_response(client_socket, 200, "application/json", history_json);
            }
            
            pthread_mutex_unlock(&data_mutex);
            
        } else if (strcmp(path, "/api/health") == 0) {
            printf("Serving health check\n");
            char buffer[256];
            time_t now = time(NULL);
            
            int server_ok = (server_socket != -1) && running;
            int data_ok = (strlen(system_json) > 0);
            
            snprintf(buffer, sizeof(buffer), 
                "{\n"
                "  \"status\": \"%s\",\n"
                "  \"service\": \"system-monitor\",\n"
                "  \"timestamp\": %ld,\n"
                "  \"server_running\": %s,\n"
                "  \"data_available\": %s\n"
                "}",
                server_ok ? "ok" : "error",
                (long)now,
                server_ok ? "true" : "false",
                data_ok ? "true" : "false");
            
            send_http_response(client_socket, 200, "application/json", buffer);
            
        } else {
            printf("404 Not Found: %s\n", path);
            const char* not_found = 
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "    <title>404 Not Found</title>\n"
                "    <style>\n"
                "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
                "        h1 { color: #cc0000; }\n"
                "        .container { max-width: 600px; margin: 0 auto; }\n"
                "        .error { background: #ffe6e6; padding: 20px; border-radius: 5px; }\n"
                "        code { background: #eee; padding: 2px 4px; border-radius: 3px; }\n"
                "    </style>\n"
                "</head>\n"
                "<body>\n"
                "    <div class=\"container\">\n"
                "        <h1>404 Not Found</h1>\n"
                "        <div class=\"error\">\n"
                "            <p>The requested URL <code>%s</code> was not found on this server.</p>\n"
                "            <p>Available endpoints:</p>\n"
                "            <ul>\n"
                "                <li><code>/api/system</code> - System information</li>\n"
                "                <li><code>/api/history</code> - System history</li>\n"
                "                <li><code>/api/health</code> - Health check</li>\n"
                "            </ul>\n"
                "        </div>\n"
                "    </div>\n"
                "</body>\n"
                "</html>";
            
            char not_found_buffer[2048];
            snprintf(not_found_buffer, sizeof(not_found_buffer), not_found, path);
            send_http_response(client_socket, 404, "text/html; charset=utf-8", not_found_buffer);
        }
        
    } else {
        printf("405 Method Not Allowed: %s\n", method);
        const char* not_allowed = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "    <title>405 Method Not Allowed</title>\n"
            "    <style>\n"
            "        body { font-family: Arial, sans-serif; margin: 40px; }\n"
            "        h1 { color: #cc6600; }\n"
            "        .container { max-width: 600px; margin: 0 auto; }\n"
            "        .warning { background: #fff3cd; padding: 20px; border-radius: 5px; }\n"
            "    </style>\n"
            "</head>\n"
            "<body>\n"
            "    <div class=\"container\">\n"
            "        <h1>405 Method Not Allowed</h1>\n"
            "        <div class=\"warning\">\n"
            "            <p>The method <code>%s</code> is not allowed for the requested URL.</p>\n"
            "            <p>This server only supports <code>GET</code> and <code>OPTIONS</code> methods.</p>\n"
            "        </div>\n"
            "    </div>\n"
            "</body>\n"
            "</html>";
        
        char not_allowed_buffer[2048];
        snprintf(not_allowed_buffer, sizeof(not_allowed_buffer), not_allowed, method);
        send_http_response(client_socket, 405, "text/html; charset=utf-8", not_allowed_buffer);
    }
    
    close(client_socket);
}

char* get_local_ip() {
    static char ip[INET_ADDRSTRLEN] = "127.0.0.1";
    struct ifaddrs *ifaddr, *ifa;
    int family;
    
    if (getifaddrs(&ifaddr) == -1) {
        return ip;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        family = ifa->ifa_addr->sa_family;
        
        if (family == AF_INET && strcmp(ifa->ifa_name, "lo") != 0) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
            
            if (strncmp(ip, "192.168.", 8) != 0 &&
                strncmp(ip, "10.", 3) != 0 &&
                strncmp(ip, "172.", 4) != 0) {
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return ip;
}

int start_server(int port) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        return -1;
    }
    
    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_socket);
        return -1;
    }
    
    if (pthread_create(&update_thread, NULL, update_data_thread, NULL) != 0) {
        perror("pthread_create");
        close(server_socket);
        return -1;
    }
    
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║      System Monitor Server v2.0         ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("✅ Server started successfully!\n");
    printf("🌐 Local:   http://localhost:%d\n", port);
    printf("🌐 Network: http://%s:%d\n", get_local_ip(), port);
    printf("📊 API:     http://localhost:%d/api/system\n", port);
    printf("🏥 Health:  http://localhost:%d/api/health\n", port);
    printf("🖥️  GPU:     Data collection enabled\n");
    printf("🛑 Press Ctrl+C to stop\n");
    printf("\n");
    
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, 
                                  (struct sockaddr *)&client_addr, 
                                  &client_len);
        
        if (client_socket < 0) {
            if (running) {
                perror("accept");
            }
            continue;
        }
        
        handle_client(client_socket);
    }
    
    return 0;
}

void stop_server() {
    running = 0;
    
    if (server_socket != -1) {
        close(server_socket);
        server_socket = -1;
    }
    
    if (update_thread) {
        pthread_join(update_thread, NULL);
    }
}