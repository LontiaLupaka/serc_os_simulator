#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

#include "../include/process.h"
#include "../include/scheduler.h"
#include "../include/deadlock.h"
#include "../include/logger.h"
#include "../include/config.h"
#include "../include/ipc.h"

#define GANTT_SAVE_FILE "gantt_history.dat"
#define GANTT_SAVE_VERSION 1
#define SCHEDULER_STATE_FILE "scheduler_state.dat"
#define SCHEDULER_STATE_VERSION 2

/* ================= GLOBALS ================= */

SchedulerStats scheduler_stats = {0};

int current_scheduler_type = SCHEDULER_HYBRID;

TimelineEntry execution_timeline[1000];
int timeline_count = 0;

int ready_queues[4][MAX_PROCESSES];
int queue_sizes[4] = {0};

/*
=====================================================
NEW:
Persistent scheduler state

This allows:
✔ interrupted processes to resume
✔ scheduler to remember running process
✔ proper preemption behavior
=====================================================
*/

static int scheduler_running_process = -1;
static int scheduler_slice = 0;
static int scheduler_last_running_process = -1;
static int scheduler_last_dispatched_pid = -1;
static int scheduler_initialized = 0;

void save_scheduler_state(void) {
    FILE *f = fopen(SCHEDULER_STATE_FILE, "w");

    if (!f)
        return;

    fprintf(f, "SCHED_SAVE %d\n", SCHEDULER_STATE_VERSION);
    fprintf(f, "%d\n", scheduler_stats.current_time);
    fprintf(f, "%d\n", scheduler_stats.total_context_switches);
    fprintf(f, "%d\n", scheduler_stats.total_processes_completed);
    fprintf(f, "%d\n", scheduler_stats.total_processes_preempted);
    fprintf(f, "%f\n", scheduler_stats.average_waiting_time);
    fprintf(f, "%f\n", scheduler_stats.average_turnaround_time);
    fprintf(f, "%f\n", scheduler_stats.cpu_utilization);
    fprintf(f, "%d\n", scheduler_last_dispatched_pid);

    fclose(f);
}

static void load_scheduler_state(void) {
    FILE *f = fopen(SCHEDULER_STATE_FILE, "r");

    if (!f)
        return;

    int version = 0;
    SchedulerStats loaded = {0};

    int scanned = fscanf(
        f,
        "SCHED_SAVE %d\n%d\n%d\n%d\n%d\n%f\n%f\n%f\n%d\n",
        &version,
        &loaded.current_time,
        &loaded.total_context_switches,
        &loaded.total_processes_completed,
        &loaded.total_processes_preempted,
        &loaded.average_waiting_time,
        &loaded.average_turnaround_time,
        &loaded.cpu_utilization,
        &scheduler_last_dispatched_pid
    );

    fclose(f);

    if ((version == 1 && scanned != 8) ||
        (version == SCHEDULER_STATE_VERSION && scanned != 9) ||
        version < 1 ||
        version > SCHEDULER_STATE_VERSION) {
        return;
    }

    if (version == 1)
        scheduler_last_dispatched_pid = -1;

    scheduler_stats = loaded;
}

static int count_completed_processes(void) {
    int completed = 0;

    for (int i = 0; i < process_count; i++) {
        if (processes[i].remaining_time <= 0 ||
            processes[i].state == PROCESS_TERMINATED) {
            completed++;
        }
    }

    return completed;
}

static void sync_completed_process_count(void) {
    int completed = count_completed_processes();

    if (scheduler_stats.total_processes_completed < completed) {
        scheduler_stats.total_processes_completed = completed;
        save_scheduler_state();
    }
}

static void record_dispatch_context_switch(int process_index) {
    int dispatched_pid = processes[process_index].pid;

    if (scheduler_last_dispatched_pid > 0 &&
        scheduler_last_dispatched_pid != dispatched_pid) {
        scheduler_stats.total_context_switches++;
    }

    scheduler_last_dispatched_pid = dispatched_pid;
    save_scheduler_state();
}

static int same_timeline_entry(TimelineEntry a, TimelineEntry b) {
    return a.pid == b.pid &&
           a.start_time == b.start_time &&
           a.end_time == b.end_time;
}

static int is_complete_timeline_entry(TimelineEntry entry) {
    return entry.end_time > entry.start_time;
}

static int is_open_timeline_entry(TimelineEntry entry) {
    return entry.end_time < 0;
}

static int read_gantt_file(TimelineEntry entries[], int max_entries) {
    FILE *f = fopen(GANTT_SAVE_FILE, "r");

    if (!f)
        return 0;

    int version = 0;
    int count = 0;
    int loaded = 0;

    if (fscanf(f, "GANTT_SAVE %d\n", &version) != 1 ||
        version != GANTT_SAVE_VERSION) {
        fclose(f);
        return 0;
    }

    if (fscanf(f, "%d\n", &count) != 1) {
        fclose(f);
        return 0;
    }

    for (int i = 0; i < count && loaded < max_entries; i++) {
        TimelineEntry entry;

        if (fscanf(
                f,
                "%d\t%d\t%d\n",
                &entry.pid,
                &entry.start_time,
                &entry.end_time) != 3) {
            break;
        }

        if (is_complete_timeline_entry(entry))
            entries[loaded++] = entry;
    }

    fclose(f);
    return loaded;
}

static void save_gantt_history(void) {
    TimelineEntry merged[1000];
    int merged_count = read_gantt_file(merged, 1000);

    for (int i = 0; i < timeline_count && merged_count < 1000; i++) {
        if (!is_complete_timeline_entry(execution_timeline[i]))
            continue;

        int exists = 0;

        for (int j = 0; j < merged_count; j++) {
            if (same_timeline_entry(execution_timeline[i], merged[j])) {
                exists = 1;
                break;
            }
        }

        if (!exists)
            merged[merged_count++] = execution_timeline[i];
    }

    FILE *f = fopen(GANTT_SAVE_FILE, "w");

    if (!f)
        return;

    fprintf(f, "GANTT_SAVE %d\n", GANTT_SAVE_VERSION);
    fprintf(f, "%d\n", merged_count);

    for (int i = 0; i < merged_count; i++) {
        fprintf(
            f,
            "%d\t%d\t%d\n",
            merged[i].pid,
            merged[i].start_time,
            merged[i].end_time
        );
    }

    fclose(f);
}

static void load_gantt_history(void) {
    TimelineEntry loaded_entries[1000];
    TimelineEntry open_entries[1000];
    int loaded_count = read_gantt_file(loaded_entries, 1000);
    int open_count = 0;

    for (int i = 0; i < timeline_count && open_count < 1000; i++) {
        if (is_open_timeline_entry(execution_timeline[i]))
            open_entries[open_count++] = execution_timeline[i];
    }

    timeline_count = 0;
    int max_end_time = 0;

    for (int i = 0; i < loaded_count && timeline_count < 1000; i++) {
        execution_timeline[timeline_count++] = loaded_entries[i];

        if (loaded_entries[i].end_time > max_end_time)
            max_end_time = loaded_entries[i].end_time;
    }

    for (int i = 0; i < open_count && timeline_count < 1000; i++)
        execution_timeline[timeline_count++] = open_entries[i];

    if (scheduler_stats.current_time < max_end_time)
        scheduler_stats.current_time = max_end_time;
}

/* ================= HELPERS ================= */

static void append_output(char *buffer, const char *format, ...) {

    va_list args;

    va_start(args, format);

    char temp[1024];

    vsnprintf(temp, sizeof(temp), format, args);

    va_end(args);

    strcat(buffer, temp);
}

static void record_timeline(int pid, int start, int end) {

    if (timeline_count >= 1000)
        return;

    execution_timeline[timeline_count].pid = pid;
    execution_timeline[timeline_count].start_time = start;
    execution_timeline[timeline_count].end_time = end;

    timeline_count++;
}

static void close_timeline(int pid, int start, int end) {
    for (int i = timeline_count - 1; i >= 0; i--) {
        if (execution_timeline[i].pid == pid &&
            is_open_timeline_entry(execution_timeline[i])) {
            execution_timeline[i].end_time = end;
            save_gantt_history();
            return;
        }
    }

    if (end > start) {
        record_timeline(pid, start, end);
        save_gantt_history();
    }
}

static void clear_ready_queues() {

    memset(queue_sizes, 0, sizeof(queue_sizes));
    memset(ready_queues, 0, sizeof(ready_queues));
}

static int already_in_queue(int pid_index, int priority) {

    for (int i = 0; i < queue_sizes[priority]; i++) {

        if (ready_queues[priority][i] == pid_index)
            return 1;
    }

    return 0;
}

static void add_to_ready(int pid_index) {

    int p = processes[pid_index].priority;

    if (p < 1)
        p = 1;

    if (p > 3)
        p = 3;

    /*
    ============================================
    IMPORTANT FIX:
    Prevent duplicate queue entries
    ============================================
    */

    if (already_in_queue(pid_index, p))
        return;

    if (queue_sizes[p] < MAX_PROCESSES) {

        ready_queues[p][queue_sizes[p]] =
            pid_index;

        queue_sizes[p]++;
    }
}

static int acquire_resources_for_dispatch(int pid_index, char *output) {
    if (request_process_resources(pid_index)) {
        processes[pid_index].resource_blocked = 0;
        return 1;
    }

    processes[pid_index].state = PROCESS_WAITING_RESOURCE;
    processes[pid_index].resource_blocked = 1;

    strcpy(
        processes[pid_index].last_event,
        "Waiting for Resources"
    );

    append_output(
        output,
        "Time %-3d -> P%d WAITING for resources (Banker's Algorithm denied unsafe request)\n",
        scheduler_stats.current_time,
        processes[pid_index].pid
    );

    ipc_handle_scheduler_event(
        "RESOURCE_WAIT",
        processes[pid_index].pid,
        "resource request denied by Banker safety check",
        scheduler_stats.current_time
    );

    LOG_WARNING(
        "DEADLOCK",
        "P%d resource request denied by Banker's Algorithm at time %d",
        processes[pid_index].pid,
        scheduler_stats.current_time
    );

    return 0;
}

static void retry_resource_waiting_processes(char *output) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].remaining_time <= 0 ||
            processes[i].state != PROCESS_WAITING_RESOURCE)
            continue;

        if (!request_process_resources(i))
            continue;

        processes[i].state = PROCESS_READY;
        processes[i].resource_blocked = 0;

        strcpy(
            processes[i].last_event,
            "Resources Allocated"
        );

        add_to_ready(i);

        ipc_handle_scheduler_event(
            "RESOURCE_READY",
            processes[i].pid,
            "resources allocated and process moved to READY",
            scheduler_stats.current_time
        );

        if (output) {
            append_output(
                output,
                "Time %-3d -> P%d resources allocated, moved to READY\n",
                scheduler_stats.current_time,
                processes[i].pid
            );
        }
    }
}

static void add_ready_processes_to_queues(void) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].remaining_time > 0 &&
            (processes[i].state == PROCESS_NEW ||
             processes[i].state == PROCESS_READY)) {
            processes[i].state = PROCESS_READY;
            add_to_ready(i);
        }
    }
}

static int get_next_process() {

    /*
    ============================================
    PRIORITY QUEUE SEARCH

    1 = highest priority
    3 = lowest priority
    ============================================
    */

    for (int pri = 1; pri <= 3; pri++) {

        while (queue_sizes[pri] > 0) {

            int idx = ready_queues[pri][0];

            /*
            REMOVE FROM QUEUE
            */

            for (int i = 0;
                 i < queue_sizes[pri] - 1;
                 i++) {

                ready_queues[pri][i] =
                    ready_queues[pri][i + 1];
            }

            queue_sizes[pri]--;

            /*
            RETURN VALID PROCESS
            */

            if (processes[idx].remaining_time > 0 &&
                processes[idx].state != PROCESS_TERMINATED) {

                return idx;
            }
        }
    }

    return -1;
}

/* ================= INIT ================= */

void initialize_scheduler() {

    memset(&scheduler_stats, 0, sizeof(scheduler_stats));
    scheduler_last_dispatched_pid = -1;
    load_scheduler_state();

    timeline_count = 0;
    load_gantt_history();

    clear_ready_queues();

    scheduler_running_process = -1;

    scheduler_last_running_process = -1;

    scheduler_slice = 0;

    scheduler_initialized = 0;
}

/* ================= RESET ================= */

void reset_scheduler_for_run() {

    load_gantt_history();
    int loaded_current_time = scheduler_stats.current_time;
    sync_completed_process_count();

    /*
    ============================================
    IMPORTANT:
    Do NOT fully reset every run

    This preserves interrupted processes
    and allows proper resume behavior
    ============================================
    */

    if (!scheduler_initialized) {

        clear_ready_queues();

        scheduler_stats.current_time = loaded_current_time;

        scheduler_stats.average_waiting_time = 0;

        scheduler_stats.average_turnaround_time = 0;

        scheduler_stats.cpu_utilization = 0;

        scheduler_running_process = -1;

        scheduler_last_running_process = -1;

        scheduler_slice = 0;

        scheduler_initialized = 1;
    }

    /*
    ============================================
    ADD READY PROCESSES
    ============================================
    */

    add_ready_processes_to_queues();
    retry_resource_waiting_processes(NULL);
}

/* ================= PRIORITY ================= */

void run_priority_scheduler(char *output) {

    append_output(
        output,
        "Priority Scheduler delegated to Hybrid Scheduler\n\n"
    );

    run_hybrid_scheduler(output);
}

/* ================= ROUND ROBIN ================= */

void run_round_robin_scheduler(char *output) {

    append_output(
        output,
        "Round Robin Scheduler delegated to Hybrid Scheduler\n\n"
    );

    run_hybrid_scheduler(output);
}

/* ================= HYBRID ================= */

void run_hybrid_scheduler(char *output) {

    int quantum = system_config.time_quantum;

    if (quantum <= 0)
        quantum = 2;

    int completed = 0;

    /*
    ============================================
    COUNT COMPLETED PROCESSES
    ============================================
    */

    for (int i = 0; i < process_count; i++) {

        if (processes[i].remaining_time <= 0 ||
            processes[i].state == PROCESS_TERMINATED) {

            completed++;
        }
    }

    append_output(
        output,
        "=========== HYBRID SCHEDULER ===========\n\n"
    );

    /*
    ============================================
    MAIN SCHEDULER LOOP
    ============================================
    */

    while (completed < process_count) {

        /*
        ============================================
        PREEMPTION CHECK

        If higher priority process arrives:
        interrupt current process
        ============================================
        */

        if (scheduler_running_process != -1) {

            int running_priority =
                processes[scheduler_running_process]
                    .priority;

            int higher_exists = 0;

            for (int p = 1;
                 p < running_priority;
                 p++) {

                if (queue_sizes[p] > 0) {

                    higher_exists = 1;

                    break;
                }
            }

            /*
            ========================================
            TRUE INTERRUPT
            ========================================
            */

            if (higher_exists) {

                append_output(
                    output,
                    "Time %-3d -> P%d INTERRUPTED by higher priority process\n",
                    scheduler_stats.current_time,
                    processes[scheduler_running_process].pid
                );

                LOG_INFO("SCHEDULER", "P%d interrupted by a higher priority process at time %d",
                    processes[scheduler_running_process].pid,
                    scheduler_stats.current_time
                );

                ipc_handle_scheduler_event(
                    "PREEMPTED",
                    processes[scheduler_running_process].pid,
                    "higher priority process entered ready queue",
                    scheduler_stats.current_time
                );

                processes[scheduler_running_process]
                    .times_preempted++;

                strcpy(
                    processes[scheduler_running_process]
                        .last_event,
                    "Interrupted"
                );

                processes[scheduler_running_process]
                    .state = PROCESS_READY;

                add_to_ready(
                    scheduler_running_process
                );

                close_timeline(
                    processes[scheduler_running_process].pid,
                    processes[scheduler_running_process].last_scheduled_time,
                    scheduler_stats.current_time
                );

                scheduler_stats
                    .total_processes_preempted++;
                save_scheduler_state();

                scheduler_running_process = -1;

                scheduler_slice = 0;
            }
        }

        if (scheduler_running_process == -1) {

            scheduler_running_process =
                get_next_process();

            if (scheduler_running_process == -1)
                break;

            if (!acquire_resources_for_dispatch(
                    scheduler_running_process,
                    output)) {
                scheduler_running_process = -1;
                scheduler_slice = 0;
                continue;
            }

            record_dispatch_context_switch(scheduler_running_process);

            processes[scheduler_running_process]
                .state = PROCESS_RUNNING;

            strcpy(
                processes[scheduler_running_process]
                    .last_event,
                "Running"
            );

            scheduler_slice = 0;

            processes[scheduler_running_process]
                .last_scheduled_time =
                    scheduler_stats.current_time;

            record_timeline(
                processes[scheduler_running_process]
                    .pid,

                scheduler_stats.current_time,

                -1
            );

            scheduler_last_running_process =
                scheduler_running_process;

            append_output(
                output,
                "Time %-3d -> Running P%-3d | Priority: %d | Remaining: %d | Memory: %d MB\n",
                scheduler_stats.current_time,
                processes[scheduler_running_process].pid,
                processes[scheduler_running_process].priority,
                processes[scheduler_running_process].remaining_time,
                processes[scheduler_running_process].memory_required
            );
        }

        processes[scheduler_running_process]
            .remaining_time--;

        scheduler_slice++;

        for (int i = 0; i < process_count; i++) {

            if (i != scheduler_running_process &&
                processes[i].remaining_time > 0 &&
                processes[i].state == PROCESS_READY) {

                processes[i].waiting_time++;

                if (strcmp(processes[i].last_event, "Interrupted") != 0) {
                    strcpy(
                        processes[i].last_event,
                        "Waiting in Ready Queue"
                    );
                }
            }
        }

        scheduler_stats.current_time++;
        save_scheduler_state();

        if (processes[scheduler_running_process]
                .remaining_time <= 0) {

            processes[scheduler_running_process]
                .remaining_time = 0;

            terminate_process(
                processes[scheduler_running_process]
                    .pid
            );

            processes[scheduler_running_process]
                .turnaround_time =
                    scheduler_stats.current_time -
                    processes[scheduler_running_process]
                        .creation_time;

            save_processes_to_file();

            close_timeline(
                processes[scheduler_running_process].pid,
                processes[scheduler_running_process].last_scheduled_time,
                scheduler_stats.current_time
            );

            append_output(
                output,
                "Time %-3d -> Process P%d COMPLETED AND SAVED\n",
                scheduler_stats.current_time,
                processes[scheduler_running_process].pid
            );

            completed++;

            retry_resource_waiting_processes(output);

            scheduler_running_process = -1;

            scheduler_slice = 0;

            continue;
        }

        int same_priority_exists = 0;

        int pr =
            processes[scheduler_running_process]
                .priority;

        for (int i = 0;
             i < queue_sizes[pr];
             i++) {

            int idx = ready_queues[pr][i];

            if (idx != scheduler_running_process &&
                processes[idx].remaining_time > 0) {

                same_priority_exists = 1;

                break;
            }
        }

        if (scheduler_slice >= quantum &&
            same_priority_exists) {

            append_output(
                output,
                "Time %-3d -> Quantum expired for P%d\n",
                scheduler_stats.current_time,
                processes[scheduler_running_process].pid
            );

            processes[scheduler_running_process]
                .state = PROCESS_READY;

            add_to_ready(
                scheduler_running_process
            );

            close_timeline(
                processes[scheduler_running_process].pid,
                processes[scheduler_running_process].last_scheduled_time,
                scheduler_stats.current_time
            );

            scheduler_running_process = -1;

            scheduler_slice = 0;
        }
    }

    if (scheduler_stats.total_processes_completed < completed)
        scheduler_stats.total_processes_completed = completed;

    save_scheduler_state();
}

int scheduler_has_active_processes() {

    for (int i = 0; i < process_count; i++) {

        if (processes[i].remaining_time > 0) {

            return 1;
        }
    }

    return 0;
}

int scheduler_step(char *output) {

    int quantum = system_config.time_quantum;

    if (quantum <= 0)
        quantum = 2;

    output[0] = '\0';

    add_ready_processes_to_queues();
    retry_resource_waiting_processes(output);

    if (!scheduler_has_active_processes()) {

        append_output(
            output,
            "All processes completed.\n"
        );

        return 0;
    }

    /* ================= PREEMPTION ================= */

    if (scheduler_running_process != -1) {

        int running_priority =
            processes[scheduler_running_process]
                .priority;

        int higher_exists = 0;

        for (int p = 1;
             p < running_priority;
             p++) {

            if (queue_sizes[p] > 0) {

                higher_exists = 1;
                break;
            }
        }

        if (higher_exists) {

            append_output(
                output,
                "Time %-3d -> P%d INTERRUPTED\n",
                scheduler_stats.current_time,
                processes[scheduler_running_process].pid
            );

            strcpy(
                processes[scheduler_running_process]
                    .last_event,
                "Interrupted"
            );

            ipc_handle_scheduler_event(
                "PREEMPTED",
                processes[scheduler_running_process].pid,
                "higher priority process entered ready queue",
                scheduler_stats.current_time
            );

            processes[scheduler_running_process]
                .times_preempted++;

            processes[scheduler_running_process]
                .state = PROCESS_READY;

            add_to_ready(
                scheduler_running_process
            );

            close_timeline(
                processes[scheduler_running_process].pid,
                processes[scheduler_running_process].last_scheduled_time,
                scheduler_stats.current_time
            );

            scheduler_stats
                .total_processes_preempted++;
            save_scheduler_state();

            scheduler_running_process = -1;
            scheduler_slice = 0;
        }
    }

    /* ================= PICK PROCESS ================= */

    if (scheduler_running_process == -1) {

        scheduler_running_process =
            get_next_process();

        if (scheduler_running_process == -1) {

            scheduler_stats.current_time++;
            save_scheduler_state();

            return 1;
        }

        if (!acquire_resources_for_dispatch(
                scheduler_running_process,
                output)) {
            scheduler_running_process = -1;
            scheduler_slice = 0;
            return 1;
        }

        record_dispatch_context_switch(scheduler_running_process);

        processes[scheduler_running_process]
            .state = PROCESS_RUNNING;

        strcpy(
            processes[scheduler_running_process]
                .last_event,
            "Running"
        );

        scheduler_slice = 0;

        processes[scheduler_running_process]
            .last_scheduled_time =
                scheduler_stats.current_time;

        record_timeline(
            processes[scheduler_running_process]
                .pid,
            scheduler_stats.current_time,
            -1
        );

        scheduler_last_running_process =
            scheduler_running_process;

        append_output(
            output,
            "Time %-3d -> Running P%-3d | Priority: %d | Remaining: %d\n",
            scheduler_stats.current_time,
            processes[scheduler_running_process].pid,
            processes[scheduler_running_process].priority,
            processes[scheduler_running_process].remaining_time
        );

        ipc_handle_scheduler_event(
            "DISPATCH",
            processes[scheduler_running_process].pid,
            "CPU dispatch granted",
            scheduler_stats.current_time
        );
        ipc_process_inbox(
            processes[scheduler_running_process].pid,
            output,
            20000
        );
    }

    /* ================= EXECUTE ================= */

    processes[scheduler_running_process]
        .remaining_time--;

    scheduler_slice++;

    for (int i = 0; i < process_count; i++) {

        if (i != scheduler_running_process &&
            processes[i].remaining_time > 0 &&
            processes[i].state == PROCESS_READY) {

            processes[i].waiting_time++;

            /* PRESERVE "INTERRUPTED" EVENT */
            if (strcmp(processes[i].last_event, "Interrupted") != 0) {
                strcpy(
                    processes[i].last_event,
                    "Waiting in Ready Queue"
                );
            }
        }
    }

    scheduler_stats.current_time++;
    save_scheduler_state();

    /* ================= COMPLETE ================= */

    if (processes[scheduler_running_process]
            .remaining_time <= 0) {

        processes[scheduler_running_process]
            .remaining_time = 0;

        terminate_process(
            processes[scheduler_running_process]
                .pid
        );

        processes[scheduler_running_process]
            .turnaround_time =
                scheduler_stats.current_time -
                processes[scheduler_running_process]
                    .creation_time;

        save_processes_to_file();

        close_timeline(
            processes[scheduler_running_process].pid,
            processes[scheduler_running_process].last_scheduled_time,
            scheduler_stats.current_time
        );

        append_output(
            output,
            "Time %-3d -> P%d TERMINATED\n",
            scheduler_stats.current_time,
            processes[scheduler_running_process].pid
        );

        ipc_handle_scheduler_event(
            "TERMINATED",
            processes[scheduler_running_process].pid,
            "process completed and released scheduler ownership",
            scheduler_stats.current_time
        );
        ipc_process_inbox(
            processes[scheduler_running_process].pid,
            output,
            20000
        );

        scheduler_stats
            .total_processes_completed++;
        save_scheduler_state();

        retry_resource_waiting_processes(output);

        scheduler_running_process = -1;
        scheduler_slice = 0;

        return 1;
    }

    /* ================= ROUND ROBIN ================= */

    int same_priority_exists = 0;

    int pr =
        processes[scheduler_running_process]
            .priority;

    for (int i = 0;
         i < queue_sizes[pr];
         i++) {

        int idx = ready_queues[pr][i];

        if (idx != scheduler_running_process &&
            processes[idx].remaining_time > 0) {

            same_priority_exists = 1;
            break;
        }
    }

    if (scheduler_slice >= quantum &&
        same_priority_exists) {

        append_output(
            output,
            "Time %-3d -> Quantum expired for P%d\n",
            scheduler_stats.current_time,
            processes[scheduler_running_process].pid
        );

        strcpy(
            processes[scheduler_running_process]
                .last_event,
            "Quantum Expired"
        );

        ipc_handle_scheduler_event(
            "QUANTUM_EXPIRED",
            processes[scheduler_running_process].pid,
            "time quantum expired, process returned to ready queue",
            scheduler_stats.current_time
        );

        processes[scheduler_running_process]
            .state = PROCESS_READY;

        add_to_ready(
            scheduler_running_process
        );

        close_timeline(
            processes[scheduler_running_process].pid,
            processes[scheduler_running_process].last_scheduled_time,
            scheduler_stats.current_time
        );

        scheduler_running_process = -1;
        scheduler_slice = 0;
    }

    return 1;
}

void generate_gantt(char *buffer) {

    append_output(
        buffer,
        "\n=========== GANTT CHART ===========\n"
    );

    if (timeline_count == 0) {
        append_output(buffer, "No CPU activity recorded.\n");
        return;
    }

    for (int i = 0; i < timeline_count; i++) {

        int end_time = execution_timeline[i].end_time;

        if (end_time < 0)
            end_time = scheduler_stats.current_time;

        append_output(
            buffer,
            "[P%d : %d -> %d] ",
            execution_timeline[i].pid,
            execution_timeline[i].start_time,
            end_time
        );
    }

    append_output(buffer, "\n");
}

void print_scheduler_stats(char *buffer) {

    sync_completed_process_count();

    int total_wait = 0;
    int total_turnaround = 0;
    int total_busy_time = 0;
    int valid_processes = 0;

    for (int i = 0; i < process_count; i++) {

        total_wait += processes[i].waiting_time;
        total_turnaround += processes[i].turnaround_time;
        valid_processes++;
    }

    for (int i = 0; i < timeline_count; i++) {

        int end_time = execution_timeline[i].end_time;

        if (end_time < 0)
            end_time = scheduler_stats.current_time;

        if (end_time > execution_timeline[i].start_time)
            total_busy_time += end_time - execution_timeline[i].start_time;
    }

    scheduler_stats.average_waiting_time =
        valid_processes ? (float)total_wait / valid_processes : 0;

    scheduler_stats.average_turnaround_time =
        valid_processes ? (float)total_turnaround / valid_processes : 0;

    scheduler_stats.cpu_utilization =
        scheduler_stats.current_time > 0
            ? ((float)total_busy_time / scheduler_stats.current_time) * 100.0f
            : 0;

    append_output(buffer, "\n=========== SCHEDULER STATS ===========\n");
    append_output(buffer, "Total Time: %d\n", scheduler_stats.current_time);
    append_output(buffer, "Average Waiting Time: %.2f\n", scheduler_stats.average_waiting_time);
    append_output(buffer, "Average Turnaround Time: %.2f\n", scheduler_stats.average_turnaround_time);
    append_output(buffer, "Processes Completed: %d\n", scheduler_stats.total_processes_completed);
    append_output(buffer, "Preemptions: %d\n", scheduler_stats.total_processes_preempted);
    append_output(buffer, "Context Switches: %d\n", scheduler_stats.total_context_switches);
    append_output(buffer, "CPU Utilization: %.2f%%\n", scheduler_stats.cpu_utilization);
}

void print_scheduler_report(char *buffer) {

    save_gantt_history();

    append_output(buffer, "\n=========== PROCESS TIMES ===========\n");

    if (process_count == 0) {
        append_output(buffer, "No processes available.\n");
    }
    else {
        append_output(
            buffer,
            "PID    Waiting Time    Turnaround Time    Preemptions\n"
        );

        for (int i = 0; i < process_count; i++) {
            append_output(
                buffer,
                "P%-5d %-15d %-18d %d\n",
                processes[i].pid,
                processes[i].waiting_time,
                processes[i].turnaround_time,
                processes[i].times_preempted
            );
        }
    }

    generate_gantt(buffer);
    print_scheduler_stats(buffer);
}

void run_scheduler(char *output) {

    if (!output)
        return;

    output[0] = '\0';

    if (process_count == 0) {
        strcpy(output, "No processes available\n");
        return;
    }

    reset_scheduler_for_run();
    LOG_INFO("SCHEDULER", "Starting scheduler type %d with %d processes", current_scheduler_type, process_count);

    if (current_scheduler_type == SCHEDULER_PRIORITY) {
        run_priority_scheduler(output);
    }
    else if (current_scheduler_type == SCHEDULER_ROUND_ROBIN) {
        run_round_robin_scheduler(output);
    }
    else {
        run_hybrid_scheduler(output);
    }

    print_scheduler_report(output);
}
