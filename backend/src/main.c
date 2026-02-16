#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "config.h"
#include "server.h"

volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
    printf("\nShutting down server...\n");
}

int main(int argc, char **argv) {
    int port = PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port. Using default: %d\n", PORT);
            port = PORT;
        }
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("================================\n");
    printf("   System Monitor Server v1.0\n");
    printf("================================\n");
    
    if (start_server(port) != 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    
    printf("Server running on http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop\n");
    
    while (running) {
        sleep(1);
    }
    
    stop_server();
    printf("Server stopped\n");
    
    return 0;
}