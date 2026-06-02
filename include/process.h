#ifndef PROCESS_H
#define PROCESS_H

#include "config.h"

/* ================= PROCESS CONTROL BLOCK ================= */

typedef struct {

    int pid;

    int parent_pid;

    int child_count;

    int active_child_count;

    int generation;

    int is_orphan;

    char type[20];

    int priority;

    int burst_time;

    int remaining_time;

    int waiting_time;

    int turnaround_time;

    int memory_required;

    /* ================= PROCESS STATE ================= */

    int state;

    int creation_time;

    int start_time;

    int completion_time;

    int last_scheduled_time;

    /* ================= SCHEDULER DATA ================= */

    int times_preempted;

    int total_wait_time;

    int is_starving;

    /* ================= RESOURCE FLAGS ================= */

    int io_pending;

    int resource_blocked;

    int assigned_core;

    /* ================= LIVE UI DATA ================= */

    char last_event[128];

    int memory_allocated;

} PCB;

/* ================= GLOBAL PROCESS TABLE ================= */

extern PCB processes[MAX_PROCESSES];

extern int process_count;

/* ================= FUNCTIONS ================= */

int add_process_auto(
    const char *type,
    int severity,
    int lives,
    int location,
    int urgency,
    int people,
    int damage
);

int add_child_process_auto(
    int parent_pid,
    const char *type,
    int severity,
    int lives,
    int location,
    int urgency,
    int people,
    int damage
);

int find_process_index_by_pid(int pid);
int process_has_active_children(int pid);

/* terminate process but KEEP IT SAVED */
void terminate_process(int pid);

/* print all saved processes */
void print_process_state(char *buffer);

/* persistence */
void save_processes_to_file(void);
void load_processes_from_file(void);

/* reset system */
void reset_system(void);

int get_process_state_string(
    int state,
    char *str
);

int check_starvation(
    int pid,
    int current_time
);

void update_process_wait_times(
    int current_time
);

#endif
