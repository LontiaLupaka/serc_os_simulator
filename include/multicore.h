#ifndef MULTICORE_H
#define MULTICORE_H

#include "process.h"

#define DEFAULT_CPU_CORES 4
#define MAX_CPU_CORES 8

typedef struct {
    int core_id;
    int current_pid;
    int idle_time;
    int total_time;
    int context_switches;
} CPUCore;

typedef struct {
    CPUCore cores[MAX_CPU_CORES];
    int num_cores;
    int total_load;
    int idle_cores;
} CPUSystem;

extern CPUSystem cpu_system;

/* Multi-core Functions */
void init_multicore_system(int num_cores);
int assign_to_idle_core(int pid);
int get_best_core_for_process(int pid);
void update_core_load();
void print_cpu_status(char *buffer);
float get_average_cpu_utilization();

#endif