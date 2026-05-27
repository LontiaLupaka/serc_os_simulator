#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "multicore.h"
#include "logger.h"

CPUSystem cpu_system = {0};

void init_multicore_system(int num_cores) {
    cpu_system.num_cores = num_cores;

    for (int i = 0; i < num_cores; i++) {
        cpu_system.cores[i].core_id = i;
        cpu_system.cores[i].current_pid = -1;
    }
}

int assign_to_idle_core(int pid) {
    for (int i = 0; i < cpu_system.num_cores; i++) {
        if (cpu_system.cores[i].current_pid == -1) {
            cpu_system.cores[i].current_pid = pid;
            return i;
        }
    }
    return -1;
}

int get_best_core_for_process(int pid) {
    (void)pid;
    return 0;
}

void update_core_load() {}

void print_cpu_status(char *buffer) {
    sprintf(buffer, "CPU STATUS\n");
}

float get_average_cpu_utilization() {
    return 0.5;
}