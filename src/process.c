#include <stdio.h>
#include <string.h>

#include "../include/process.h"
#include "../include/memory.h"
#include "../include/deadlock.h"
#include "../include/logger.h"
#include "../include/config.h"
#include "../include/scheduler.h"

#define PROCESS_SAVE_FILE "processes.dat"
#define PROCESS_SAVE_VERSION 1

PCB processes[MAX_PROCESSES];
int process_count = 0;

/* UNIQUE PID GENERATOR */
static int next_pid = 1;

/* ---------- PRIORITY CALCULATION ---------- */

int calculate_priority(
    int severity,
    int lives,
    int location,
    int urgency,
    int people,
    int damage
) {

    int score = 0;

    if (lives)
        score += 40;

    score += severity * 4;
    score += urgency * 12;
    score += people * 2;
    score += damage * 5;

    if (location == 2)
        score += 10;

    /* LOWER VALUE = HIGHER PRIORITY */

    if (score >= CRITICAL_SCORE_THRESHOLD)
        return PRIORITY_CRITICAL;

    if (score >= HIGH_SCORE_THRESHOLD)
        return PRIORITY_HIGH;

    return PRIORITY_NORMAL;
}

static void normalize_loaded_process_state(PCB *p) {
    if (p->remaining_time > 0 && p->state == PROCESS_RUNNING)
        p->state = PROCESS_READY;
}

static int estimate_saved_vehicles(const PCB *p) {
    int priority_boost = PRIORITY_NORMAL - p->priority;

    if (priority_boost < 0)
        priority_boost = 0;

    return 1 + priority_boost + (p->memory_required / 80);
}

static int estimate_saved_channels(const PCB *p) {
    return 1 + (p->memory_required / 120);
}

void save_processes_to_file(void) {
    FILE *f = fopen(PROCESS_SAVE_FILE, "w");

    if (!f)
        return;

    fprintf(f, "PROC_SAVE %d\n", PROCESS_SAVE_VERSION);
    fprintf(f, "%d\n", next_pid);

    fprintf(f, "%d\n", process_count);

    for (int i = 0; i < process_count; i++) {
        PCB *p = &processes[i];

        fprintf(
            f,
            "%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n",
            p->pid,
            p->type,
            p->priority,
            p->burst_time,
            p->remaining_time,
            p->waiting_time,
            p->turnaround_time,
            p->memory_required,
            p->state,
            p->creation_time,
            p->completion_time,
            p->last_scheduled_time,
            p->times_preempted,
            p->total_wait_time,
            p->is_starving,
            p->io_pending,
            p->resource_blocked,
            p->assigned_core,
            p->memory_allocated,
            p->last_event
        );
    }

    fclose(f);
}

void load_processes_from_file(void) {
    FILE *f = fopen(PROCESS_SAVE_FILE, "r");

    if (!f)
        return;

    int version = 0;
    int count = 0;

    if (fscanf(f, "PROC_SAVE %d\n", &version) != 1 ||
        version != PROCESS_SAVE_VERSION) {
        fclose(f);
        return;
    }

    if (fscanf(f, "%d\n", &next_pid) != 1) {
        next_pid = 1;
    }

    if (fscanf(f, "%d\n", &count) != 1) {
        fclose(f);
        return;
    }

    process_count = 0;

    for (int i = 0; i < count && process_count < MAX_PROCESSES; i++) {
        PCB p;
        memset(&p, 0, sizeof(PCB));

        int scanned = fscanf(
            f,
            "%d\t%19[^\t]\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%127[^\n]\n",
            &p.pid,
            p.type,
            &p.priority,
            &p.burst_time,
            &p.remaining_time,
            &p.waiting_time,
            &p.turnaround_time,
            &p.memory_required,
            &p.state,
            &p.creation_time,
            &p.completion_time,
            &p.last_scheduled_time,
            &p.times_preempted,
            &p.total_wait_time,
            &p.is_starving,
            &p.io_pending,
            &p.resource_blocked,
            &p.assigned_core,
            &p.memory_allocated,
            p.last_event
        );

        if (scanned != 20)
            continue;

        normalize_loaded_process_state(&p);
        processes[process_count++] = p;

        if (next_pid <= p.pid)
            next_pid = p.pid + 1;

        if (p.memory_allocated > 0)
            allocate_memory(p.memory_allocated);

        if (p.remaining_time > 0 &&
            p.state != PROCESS_TERMINATED) {

            register_process_resources(
                process_count - 1,
                p.memory_required,
                estimate_saved_vehicles(&p),
                estimate_saved_channels(&p)
            );
        }
    }

    fclose(f);
}

/* ---------- ADD PROCESS ---------- */

int add_process_auto(
    const char *type,
    int severity,
    int lives,
    int location,
    int urgency,
    int people,
    int damage
) {

    if (process_count >= MAX_PROCESSES) {

        printf("Process limit reached!\n");
        return 0;
    }

    int memory_required = 20 + (severity * 3);

    int vehicles_required = 1 + (severity / 4) + (lives ? 1 : 0);

    int channels_required = 1 + (urgency / 2) + (people / 25);

    if (vehicles_required > total_resources[RESOURCE_VEHICLE] ||
        channels_required > total_resources[RESOURCE_CHANNEL]) {

        printf("❌ Process rejected (resource demand exceeds system capacity)\n");
        return 0;
    }

    if (!can_allocate_resources(
            process_count,
            memory_required,
            vehicles_required,
            channels_required)) {

        printf("❌ Process rejected (Banker's Algorithm unsafe admission)\n");
        LOG_WARNING(
            "DEADLOCK",
            "Rejected emergency process '%s': unsafe resource demand memory=%d vehicles=%d channels=%d",
            type,
            memory_required,
            vehicles_required,
            channels_required
        );
        return 0;
    }

    /* MEMORY CHECK */

    if (!allocate_memory(memory_required)) {

        printf("❌ Memory allocation failed\n");
        LOG_WARNING("PROCESS", "Failed to allocate %dMB for emergency process '%s'", memory_required, type);
        return 0;
    }

    register_process_resources(
        process_count,
        memory_required,
        vehicles_required,
        channels_required
    );

    PCB p;

    memset(&p, 0, sizeof(PCB));

    /* UNIQUE PID */

    p.pid = next_pid++;

    strncpy(
        p.type,
        type,
        sizeof(p.type) - 1
    );

    p.priority = calculate_priority(
        severity,
        lives,
        location,
        urgency,
        people,
        damage
    );

    /*
       DYNAMIC CPU BURST TIME
    */

       p.burst_time =
        4 +
        severity +
        urgency +
        (damage / 2);


    p.remaining_time = p.burst_time;

    p.waiting_time = 0;

    p.turnaround_time = 0;

    /*
       TRUE ARRIVAL TIME
    */

    p.creation_time =
        scheduler_stats.current_time;

    p.state = PROCESS_NEW;

    p.memory_required =
        memory_required;

    p.memory_allocated =
        memory_required;

    p.resource_blocked = 0;

    strcpy(p.last_event, "Process Created");

    /* STORE PROCESS */

    processes[process_count] = p;

    process_count++;

    save_processes_to_file();
    LOG_INFO("PROCESS", "Created process P%d type=%s priority=%d memory=%d burst=%d", p.pid, p.type, p.priority, p.memory_required, p.burst_time);

    /* OUTPUT */

    printf("\n========== PROCESS CREATED ==========\n");

    printf(
        "PID: %d\n",
        p.pid
    );

    printf(
        "Type: %s\n",
        p.type
    );

    printf(
        "Priority: %d\n",
        p.priority
    );

    printf(
        "Memory Allocated: %d MB\n",
        p.memory_required
    );

    printf(
        "Resource Demand: %d vehicles, %d communication channels\n",
        vehicles_required,
        channels_required
    );

    printf(
        "CPU Burst Time: %d\n",
        p.burst_time
    );

    printf(
        "Remaining CPU Time: %d\n",
        p.remaining_time
    );

    printf(
        "Arrival Time: %d\n",
        p.creation_time
    );

    if (p.priority == PRIORITY_CRITICAL)
        printf("🔴 PRIORITY LEVEL: CRITICAL\n");

    else if (p.priority == PRIORITY_HIGH)
        printf("🟠 PRIORITY LEVEL: HIGH\n");

    else
        printf("🟢 PRIORITY LEVEL: NORMAL\n");

    printf("=====================================\n");

    return 1;
}

/* ---------- TERMINATE PROCESS ---------- */
/* COMPLETED PROCESSES REMAIN SAVED */

void terminate_process(int pid) {

    for (int i = 0; i < process_count; i++) {

        if (processes[i].pid == pid) {

            processes[i].state = PROCESS_TERMINATED;

            strcpy(
                processes[i].last_event,
                "Process Completed"
            );

            processes[i].completion_time =
                scheduler_stats.current_time;

            /* LIVE DEALLOCATION */

            if (processes[i].memory_allocated > 0) {

                deallocate_memory(
                    processes[i].memory_allocated
                );

                processes[i].memory_allocated = 0;
            }

            release_resources(i);

            save_processes_to_file();

            LOG_INFO(
                "SCHEDULER",
                "Process P%d completed at time %d",
                pid,
                scheduler_stats.current_time
            );

            return;
        }
    }
}

/* ---------- PRINT PROCESS STATE ---------- */

void print_process_state(char *buffer) {

    char temp[512];

    sprintf(
        temp,
        "\n=========== PROCESS LIST ===========\n"
    );

    strcat(buffer, temp);

    for (int i = 0; i < process_count; i++) {

        char state_str[32];

        get_process_state_string(
            processes[i].state,
            state_str
        );

        sprintf(
            temp,

            "PID %-3d | "
            "Type %-12s | "
            "Priority %-2d | "
            "State %-12s | "
            "Burst %-3d | "
            "Remaining %-3d | "
            "Memory %-3d MB\n",

            processes[i].pid,
            processes[i].type,
            processes[i].priority,
            state_str,
            processes[i].burst_time,
            processes[i].remaining_time,
            processes[i].memory_required
        );

        strcat(buffer, temp);
    }

    strcat(
        buffer,
        "====================================\n"
    );
}

/* ---------- STATE STRING ---------- */

int get_process_state_string(
    int state,
    char *str
) {

    switch (state) {

        case PROCESS_NEW:
            strcpy(str, "NEW");
            break;

        case PROCESS_READY:
            strcpy(str, "READY");
            break;

        case PROCESS_RUNNING:
            strcpy(str, "RUNNING");
            break;

        case PROCESS_WAITING_IO:
            strcpy(str, "WAIT_IO");
            break;

        case PROCESS_WAITING_RESOURCE:
            strcpy(str, "WAIT_RES");
            break;

        case PROCESS_TERMINATED:
            strcpy(str, "TERMINATED");
            break;

        default:
            strcpy(str, "UNKNOWN");
            break;
    }

    return 1;
}

/* ---------- STARVATION CHECK ---------- */

int check_starvation(
    int pid,
    int current_time
) {

    (void)pid;
    (void)current_time;

    return 0;
}

/* ---------- UPDATE WAIT TIMES ---------- */

void update_process_wait_times(
    int current_time
) {

    for (int i = 0; i < process_count; i++) {

        if (processes[i].state ==
            PROCESS_READY) {

            processes[i].waiting_time++;
        }
    }

    (void)current_time;
}

/* ---------- RESET SYSTEM ---------- */

void reset_system(void) {

    /* Clear all processes from memory */
    memset(processes, 0, sizeof(processes));

    /* Reset process count */
    process_count = 0;

    /* Reset PID generator */
    next_pid = 1;

    /* Remove persistent save file */
    remove(PROCESS_SAVE_FILE);
    remove("gantt_history.dat");
    logger_clear_logs();
    LOG_WARNING("SYSTEM", "System reset: clearing process, memory, scheduler state, and logs");

    /* Reset memory state */
    reset_memory_system();

    /* Reset deadlock/resource state */
    init_deadlock_detection();

    /* Reset scheduler state */
    initialize_scheduler();
}
