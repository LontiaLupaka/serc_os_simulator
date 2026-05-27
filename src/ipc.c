#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../include/ipc.h"

static MessageQueue mq = {0};
static Pipe pipes[MAX_PIPES] = {0};
static int pipe_count = 0;
static SharedMemory shared_mem = {0};

static int global_time = 0;

void init_ipc_system() {
    mq.head = 0;
    mq.tail = 0;
    mq.count = 0;

    shared_mem.process_count = 0;
    shared_mem.total_memory_used = 0;
    shared_mem.active_processes = 0;
    strcpy(shared_mem.coordinator_status, "SYSTEM");
}

int enqueue_message(int sender, int receiver, const char *msg_type, const char *payload, int priority) {
    if (mq.count >= MAX_MESSAGES) return 0;

    Message *msg = &mq.messages[mq.tail];
    msg->sender_pid = sender;
    msg->receiver_pid = receiver;
    strcpy(msg->message_type, msg_type);
    strcpy(msg->payload, payload);
    msg->timestamp = global_time++;
    msg->priority = priority;

    mq.tail = (mq.tail + 1) % MAX_MESSAGES;
    mq.count++;

    return 1;
}

int dequeue_message(int pid, Message *out_msg) {
    if (mq.count == 0) return 0;

    for (int i = 0; i < mq.count; i++) {
        int idx = (mq.head + i) % MAX_MESSAGES;
        if (mq.messages[idx].receiver_pid == pid) {
            *out_msg = mq.messages[idx];
            return 1;
        }
    }
    return 0;
}

int create_pipe(int producer_pid, int consumer_pid) {
    if (pipe_count >= MAX_PIPES) return -1;

    Pipe *p = &pipes[pipe_count];
    p->producer_pid = producer_pid;
    p->consumer_pid = consumer_pid;
    p->has_data = 0;

    return pipe_count++;
}

int write_to_pipe(int pipe_id, const char *data) {
    if (pipe_id < 0 || pipe_id >= pipe_count) return 0;

    strcpy(pipes[pipe_id].buffer, data);
    pipes[pipe_id].has_data = 1;

    return 1;
}

int read_from_pipe(int pipe_id, char *data) {
    if (pipe_id < 0 || pipe_id >= pipe_count) return 0;

    if (!pipes[pipe_id].has_data) return 0;

    strcpy(data, pipes[pipe_id].buffer);
    pipes[pipe_id].has_data = 0;

    return 1;
}

SharedMemory* get_shared_memory() {
    return &shared_mem;
}

void update_coordinator_status(const char *status) {
    strncpy(shared_mem.coordinator_status, status,
            sizeof(shared_mem.coordinator_status));
}

const char* simulate_process_coordination(int initiator_pid, const char *emergency_type) {

    (void)emergency_type;  // ✅ FIX: suppress unused warning

    static char report[2048];
    memset(report, 0, sizeof(report));

    sprintf(report, "IPC REPORT\n");

    enqueue_message(initiator_pid, 2, "ALERT", "msg", 1);
    enqueue_message(initiator_pid, 3, "ALERT", "msg", 1);

    int pipe_1 = create_pipe(initiator_pid, 2);
    write_to_pipe(pipe_1, "DATA");

    update_coordinator_status("ACTIVE");

    return report;
}

const char* send_message(int sender, int receiver, const char *msg) {
    static char buffer[256];

    enqueue_message(sender, receiver, "DATA", msg, 2);

    sprintf(buffer, "P%d → P%d: %s", sender, receiver, msg);

    return buffer;
}

const char* get_ipc_report() {
    static char report[1024];
    sprintf(report, "IPC STATUS REPORT\n");
    return report;
}