#ifndef SCHEDULER_H 
#define SCHEDULER_H

#include "config.h"

typedef struct {
    int current_time;
    int total_context_switches;
    float average_waiting_time;
    float average_turnaround_time;
    float cpu_utilization;
    int total_processes_completed;
    int total_processes_preempted;
} SchedulerStats;

typedef struct {
    int pid;
    int start_time;
    int end_time;
} TimelineEntry;

extern SchedulerStats scheduler_stats;
extern int current_scheduler_type;
extern TimelineEntry execution_timeline[1000];
extern int timeline_count;

int scheduler_step(char *output);
int scheduler_has_active_processes();

void run_scheduler(char *output);
void run_priority_scheduler(char *output);
void run_round_robin_scheduler(char *output);
void run_hybrid_scheduler(char *output);
void generate_gantt(char *buffer);
void print_scheduler_stats(char *buffer);
void print_scheduler_report(char *buffer);
void initialize_scheduler();
void reset_scheduler_for_run();

#endif
