#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../include/process.h"
#include "../include/deadlock.h"
#include "../include/memory.h"
#include "../include/config.h"

/* ================= RESOURCE STATE ================= */

ResourceAllocation resource_state[MAX_PROCESSES];

/* Memory is allocated at admission. Vehicles/channels are requested at dispatch. */
int total_resources[NUM_RESOURCES] = {TOTAL_MEMORY, 24, 12};
int available_resources[NUM_RESOURCES] = {TOTAL_MEMORY, 24, 12};

static void get_current_available_resources(int available[NUM_RESOURCES]) {
    available[RESOURCE_MEMORY] = get_memory_free();
    available[RESOURCE_VEHICLE] = available_resources[RESOURCE_VEHICLE];
    available[RESOURCE_CHANNEL] = available_resources[RESOURCE_CHANNEL];
}

static void append_line(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    size_t used = strlen(buffer);

    if (used >= size - 1)
        return;

    va_start(args, format);
    vsnprintf(buffer + used, size - used, format, args);
    va_end(args);
}

static int banker_is_safe(
    ResourceAllocation state[MAX_PROCESSES],
    int available[NUM_RESOURCES],
    int process_limit,
    int sequence[MAX_PROCESSES],
    int *sequence_count
) {
    int work[NUM_RESOURCES];
    int finish[MAX_PROCESSES] = {0};

    *sequence_count = 0;

    for (int r = 0; r < NUM_RESOURCES; r++)
        work[r] = available[r];

    for (int i = 0; i < process_limit; i++) {
        if (i < process_count &&
            (processes[i].state == PROCESS_TERMINATED ||
             processes[i].remaining_time <= 0)) {
            finish[i] = 1;
        }
    }

    int progressed = 1;

    while (progressed) {
        progressed = 0;

        for (int i = 0; i < process_limit; i++) {
            if (finish[i])
                continue;

            int can_finish = 1;

            for (int r = 0; r < NUM_RESOURCES; r++) {
                if (state[i].need[r] > work[r]) {
                    can_finish = 0;
                    break;
                }
            }

            if (can_finish) {
                for (int r = 0; r < NUM_RESOURCES; r++)
                    work[r] += state[i].allocated[r];

                finish[i] = 1;
                sequence[*sequence_count] =
                    i < process_count ? processes[i].pid : i + 1;
                (*sequence_count)++;
                progressed = 1;
            }
        }
    }

    for (int i = 0; i < process_limit; i++) {
        if (!finish[i])
            return 0;
    }

    return 1;
}

/* ================= INITIALIZATION ================= */

void init_deadlock_detection() {

    /* reset resource tracking */
    for (int i = 0; i < MAX_PROCESSES; i++) {

        for (int j = 0; j < NUM_RESOURCES; j++) {
            resource_state[i].allocated[j] = 0;
            resource_state[i].maximum[j]   = 0;
            resource_state[i].need[j]      = 0;
        }
    }

    total_resources[RESOURCE_MEMORY] = TOTAL_MEMORY;
    total_resources[RESOURCE_VEHICLE] = 24;
    total_resources[RESOURCE_CHANNEL] = 12;

    available_resources[RESOURCE_MEMORY] = TOTAL_MEMORY;
    available_resources[RESOURCE_VEHICLE] = total_resources[RESOURCE_VEHICLE];
    available_resources[RESOURCE_CHANNEL] = total_resources[RESOURCE_CHANNEL];
}

/* ================= RESOURCE CHECK ================= */

int can_allocate_resources(int process_index, int memory, int vehicles, int channels) {

    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return 0;

    if (memory < 0 || vehicles < 0 || channels < 0)
        return 0;

    int current_available[NUM_RESOURCES];
    get_current_available_resources(current_available);

    if (memory > current_available[RESOURCE_MEMORY])
        return 0;

    if (vehicles > total_resources[RESOURCE_VEHICLE])
        return 0;

    if (channels > total_resources[RESOURCE_CHANNEL])
        return 0;

    ResourceAllocation simulated[MAX_PROCESSES];
    int simulated_available[NUM_RESOURCES];
    int sequence[MAX_PROCESSES];
    int sequence_count = 0;

    memcpy(simulated, resource_state, sizeof(simulated));
    memcpy(simulated_available, current_available, sizeof(simulated_available));

    simulated[process_index].allocated[RESOURCE_MEMORY] = memory;
    simulated[process_index].allocated[RESOURCE_VEHICLE] = 0;
    simulated[process_index].allocated[RESOURCE_CHANNEL] = 0;

    simulated[process_index].maximum[RESOURCE_MEMORY] = memory;
    simulated[process_index].maximum[RESOURCE_VEHICLE] = vehicles;
    simulated[process_index].maximum[RESOURCE_CHANNEL] = channels;

    for (int r = 0; r < NUM_RESOURCES; r++) {
        simulated[process_index].need[r] =
            simulated[process_index].maximum[r] -
            simulated[process_index].allocated[r];
    }

    simulated_available[RESOURCE_MEMORY] -= memory;

    int simulated_process_limit = process_count;

    if (process_index + 1 > simulated_process_limit)
        simulated_process_limit = process_index + 1;

    if (!banker_is_safe(
            simulated,
            simulated_available,
            simulated_process_limit,
            sequence,
            &sequence_count))
        return 0;

    return 1;
}

void register_process_resources(int process_index, int memory, int vehicles, int channels) {
    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return;

    resource_state[process_index].allocated[RESOURCE_MEMORY] = memory;
    resource_state[process_index].allocated[RESOURCE_VEHICLE] = 0;
    resource_state[process_index].allocated[RESOURCE_CHANNEL] = 0;

    resource_state[process_index].maximum[RESOURCE_MEMORY] = memory;
    resource_state[process_index].maximum[RESOURCE_VEHICLE] = vehicles;
    resource_state[process_index].maximum[RESOURCE_CHANNEL] = channels;

    for (int r = 0; r < NUM_RESOURCES; r++) {
        resource_state[process_index].need[r] =
            resource_state[process_index].maximum[r] -
            resource_state[process_index].allocated[r];
    }

    available_resources[RESOURCE_MEMORY] = get_memory_free();
}

int process_holds_resources(int process_index) {
    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return 0;

    return resource_state[process_index].allocated[RESOURCE_VEHICLE] > 0 ||
           resource_state[process_index].allocated[RESOURCE_CHANNEL] > 0;
}

int request_process_resources(int process_index) {
    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return 0;

    int request[NUM_RESOURCES] = {0};
    request[RESOURCE_VEHICLE] = resource_state[process_index].need[RESOURCE_VEHICLE];
    request[RESOURCE_CHANNEL] = resource_state[process_index].need[RESOURCE_CHANNEL];

    if (request[RESOURCE_VEHICLE] <= 0 &&
        request[RESOURCE_CHANNEL] <= 0)
        return 1;

    int current_available[NUM_RESOURCES];
    get_current_available_resources(current_available);

    if (request[RESOURCE_VEHICLE] > current_available[RESOURCE_VEHICLE] ||
        request[RESOURCE_CHANNEL] > current_available[RESOURCE_CHANNEL])
        return 0;

    ResourceAllocation simulated[MAX_PROCESSES];
    int simulated_available[NUM_RESOURCES];
    int sequence[MAX_PROCESSES];
    int sequence_count = 0;

    memcpy(simulated, resource_state, sizeof(simulated));
    memcpy(simulated_available, current_available, sizeof(simulated_available));

    for (int r = 0; r < NUM_RESOURCES; r++) {
        simulated[process_index].allocated[r] += request[r];
        simulated[process_index].need[r] -= request[r];
        simulated_available[r] -= request[r];
    }

    if (!banker_is_safe(
            simulated,
            simulated_available,
            process_count,
            sequence,
            &sequence_count))
        return 0;

    for (int r = 0; r < NUM_RESOURCES; r++) {
        resource_state[process_index].allocated[r] += request[r];
        resource_state[process_index].need[r] -= request[r];
    }

    available_resources[RESOURCE_MEMORY] = get_memory_free();
    available_resources[RESOURCE_VEHICLE] -= request[RESOURCE_VEHICLE];
    available_resources[RESOURCE_CHANNEL] -= request[RESOURCE_CHANNEL];

    return 1;
}

void allocate_resources(int process_index, int memory, int vehicles, int channels) {
    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return;

    register_process_resources(process_index, memory, vehicles, channels);

    resource_state[process_index].allocated[RESOURCE_VEHICLE] = vehicles;
    resource_state[process_index].allocated[RESOURCE_CHANNEL] = channels;
    resource_state[process_index].need[RESOURCE_VEHICLE] = 0;
    resource_state[process_index].need[RESOURCE_CHANNEL] = 0;

    available_resources[RESOURCE_MEMORY] = get_memory_free();
    available_resources[RESOURCE_VEHICLE] -= vehicles;
    available_resources[RESOURCE_CHANNEL] -= channels;

    if (available_resources[RESOURCE_VEHICLE] < 0)
        available_resources[RESOURCE_VEHICLE] = 0;

    if (available_resources[RESOURCE_CHANNEL] < 0)
        available_resources[RESOURCE_CHANNEL] = 0;
}

void release_resources(int process_index) {
    if (process_index < 0 || process_index >= MAX_PROCESSES)
        return;

    available_resources[RESOURCE_MEMORY] = get_memory_free();

    available_resources[RESOURCE_VEHICLE] +=
        resource_state[process_index].allocated[RESOURCE_VEHICLE];

    available_resources[RESOURCE_CHANNEL] +=
        resource_state[process_index].allocated[RESOURCE_CHANNEL];

    if (available_resources[RESOURCE_VEHICLE] > total_resources[RESOURCE_VEHICLE])
        available_resources[RESOURCE_VEHICLE] = total_resources[RESOURCE_VEHICLE];

    if (available_resources[RESOURCE_CHANNEL] > total_resources[RESOURCE_CHANNEL])
        available_resources[RESOURCE_CHANNEL] = total_resources[RESOURCE_CHANNEL];

    available_resources[RESOURCE_MEMORY] = get_memory_free();

    for (int r = 0; r < NUM_RESOURCES; r++) {
        resource_state[process_index].allocated[r] = 0;
        resource_state[process_index].maximum[r] = 0;
        resource_state[process_index].need[r] = 0;
    }
}

/* ================= DEADLOCK STATUS ================= */

const char* detect_deadlock() {

    static char status[1200];
    status[0] = '\0';

    int current_available[NUM_RESOURCES];
    get_current_available_resources(current_available);

    int used_memory = get_memory_used();
    int free_memory = get_memory_free();
    int sequence[MAX_PROCESSES];
    int sequence_count = 0;
    int safe = banker_is_safe(
        resource_state,
        current_available,
        process_count,
        sequence,
        &sequence_count
    );

    append_line(status, sizeof(status),
        "Banker's Algorithm State: %s\n",
        safe ? "SAFE" : "UNSAFE");

    append_line(status, sizeof(status),
        "Deadlock Status: %s\n",
        safe ? "No deadlock detected" : "Deadlock risk detected");

    append_line(status, sizeof(status),
        "Memory Allocation Strategy: %s\n",
        get_memory_allocation_strategy_name());

    append_line(status, sizeof(status),
        "Total Memory: %d MB\n",
        TOTAL_MEMORY);

    append_line(status, sizeof(status),
        "Used Memory: %d MB\n",
        used_memory);

    append_line(status, sizeof(status),
        "Free Memory: %d MB\n",
        free_memory);

    return status;
}
