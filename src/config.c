#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

SystemConfig system_config = {
    .total_memory = TOTAL_MEMORY,
    .total_cpu_units = TOTAL_CPU_UNITS,
    .total_io_units = TOTAL_IO_UNITS,
    .num_cpu_cores = 4,
    .time_quantum = DEFAULT_TIME_QUANTUM,
    .enable_io_simulation = 1,
    .enable_starvation_detection = 1,
    .enable_logging = 1,
    .log_file = "system_log.txt"
};

void load_system_config() {
    FILE *config_file = fopen("config.ini", "r");
    
    if (!config_file) {
        /* Use default config if file doesn't exist */
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), config_file)) {
        char key[128], value[128];
        
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "num_cpu_cores") == 0) {
                system_config.num_cpu_cores = atoi(value);
            } else if (strcmp(key, "time_quantum") == 0) {
                system_config.time_quantum = atoi(value);
            } else if (strcmp(key, "enable_io_simulation") == 0) {
                system_config.enable_io_simulation = atoi(value);
            } else if (strcmp(key, "enable_starvation_detection") == 0) {
                system_config.enable_starvation_detection = atoi(value);
            } else if (strcmp(key, "enable_logging") == 0) {
                system_config.enable_logging = atoi(value);
            }
        }
    }
    
    fclose(config_file);
}

void print_system_config() {
    printf("=== System Configuration ===\n");
    printf("Total Memory: %d MB\n", system_config.total_memory);
    printf("CPU Units: %d\n", system_config.total_cpu_units);
    printf("I/O Units: %d\n", system_config.total_io_units);
    printf("CPU Cores: %d\n", system_config.num_cpu_cores);
    printf("Time Quantum: %d ms\n", system_config.time_quantum);
    printf("I/O Simulation: %s\n", system_config.enable_io_simulation ? "Enabled" : "Disabled");
    printf("Starvation Detection: %s\n", system_config.enable_starvation_detection ? "Enabled" : "Disabled");
    printf("Logging: %s\n", system_config.enable_logging ? "Enabled" : "Disabled");
    printf("===========================\n");
}