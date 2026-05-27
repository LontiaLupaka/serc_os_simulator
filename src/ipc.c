#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../include/ipc.h"
#include "../include/logger.h"

#define IPC_LOG_SAVE_VERSION 1

static MessageQueue mq = {0};
static Pipe pipes[MAX_PIPES] = {0};
static int pipe_count = 0;
static SharedMemory shared_mem = {0};
static IpcLogEntry ipc_logs[MAX_IPC_LOGS] = {0};
static int ipc_log_start = 0;
static int ipc_log_count = 0;

static int global_time = 0;

void update_coordinator_status(const char *status);

static void safe_copy(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0)
        return;

    if (!src)
        src = "";

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static void append_text(char *buffer, size_t buffer_size, const char *text) {
    if (!buffer || buffer_size == 0 || !text)
        return;

    size_t used = strlen(buffer);

    if (used >= buffer_size - 1)
        return;

    strncat(buffer, text, buffer_size - used - 1);
}

static void record_ipc_log(
    int timestamp,
    int sender,
    int receiver,
    const char *event,
    const char *msg_type,
    const char *payload,
    const char *status
) {
    int slot;

    if (ipc_log_count < MAX_IPC_LOGS) {
        slot = (ipc_log_start + ipc_log_count) % MAX_IPC_LOGS;
        ipc_log_count++;
    } else {
        slot = ipc_log_start;
        ipc_log_start = (ipc_log_start + 1) % MAX_IPC_LOGS;
    }

    IpcLogEntry *entry = &ipc_logs[slot];

    entry->timestamp = timestamp;
    entry->sender_pid = sender;
    entry->receiver_pid = receiver;
    safe_copy(entry->event, sizeof(entry->event), event);
    safe_copy(entry->message_type, sizeof(entry->message_type), msg_type);
    safe_copy(entry->payload, sizeof(entry->payload), payload);
    safe_copy(entry->status, sizeof(entry->status), status);
}

static void save_ipc_logs(void) {
    FILE *f = fopen(IPC_LOG_SAVE_FILE, "w");

    if (!f)
        return;

    fprintf(f, "IPC_LOG_SAVE %d\n", IPC_LOG_SAVE_VERSION);
    fprintf(f, "%d\n", ipc_log_count);

    for (int i = 0; i < ipc_log_count; i++) {
        int idx = (ipc_log_start + i) % MAX_IPC_LOGS;
        IpcLogEntry *entry = &ipc_logs[idx];

        fprintf(
            f,
            "%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
            entry->timestamp,
            entry->sender_pid,
            entry->receiver_pid,
            entry->event,
            entry->message_type,
            entry->status,
            entry->payload
        );
    }

    fclose(f);
}

static void append_persistent_ipc_log(
    int timestamp,
    int sender,
    int receiver,
    const char *event,
    const char *msg_type,
    const char *payload,
    const char *status
) {
    record_ipc_log(timestamp, sender, receiver, event, msg_type, payload, status);
    save_ipc_logs();
}

static void load_ipc_logs(void) {
    FILE *f = fopen(IPC_LOG_SAVE_FILE, "r");

    if (!f)
        return;

    int version = 0;
    int count = 0;

    if (fscanf(f, "IPC_LOG_SAVE %d\n", &version) != 1 ||
        version != IPC_LOG_SAVE_VERSION ||
        fscanf(f, "%d\n", &count) != 1) {
        fclose(f);
        return;
    }

    for (int i = 0; i < count && i < MAX_IPC_LOGS; i++) {
        IpcLogEntry entry;
        char line[1024];

        if (!fgets(line, sizeof(line), f))
            break;

        line[strcspn(line, "\n")] = '\0';

        char *timestamp = strtok(line, "\t");
        char *sender = strtok(NULL, "\t");
        char *receiver = strtok(NULL, "\t");
        char *event = strtok(NULL, "\t");
        char *msg_type = strtok(NULL, "\t");
        char *status = strtok(NULL, "\t");
        char *payload = strtok(NULL, "");

        if (!timestamp || !sender || !receiver || !event ||
            !msg_type || !status || !payload)
            continue;

        memset(&entry, 0, sizeof(entry));
        entry.timestamp = atoi(timestamp);
        entry.sender_pid = atoi(sender);
        entry.receiver_pid = atoi(receiver);
        safe_copy(entry.event, sizeof(entry.event), event);
        safe_copy(entry.message_type, sizeof(entry.message_type), msg_type);
        safe_copy(entry.status, sizeof(entry.status), status);
        safe_copy(entry.payload, sizeof(entry.payload), payload);

        ipc_logs[ipc_log_count++] = entry;

        if (entry.timestamp >= global_time)
            global_time = entry.timestamp + 1;
    }

    fclose(f);
}

void init_ipc_system() {
    mq.head = 0;
    mq.tail = 0;
    mq.count = 0;
    pipe_count = 0;
    memset(pipes, 0, sizeof(pipes));
    memset(ipc_logs, 0, sizeof(ipc_logs));
    ipc_log_start = 0;
    ipc_log_count = 0;
    global_time = 0;

    shared_mem.process_count = 0;
    shared_mem.total_memory_used = 0;
    shared_mem.active_processes = 0;
    strcpy(shared_mem.coordinator_status, "SYSTEM");

    load_ipc_logs();

    append_persistent_ipc_log(
        global_time++,
        IPC_SYSTEM_PID,
        IPC_SYSTEM_PID,
        "INIT",
        "SYSTEM",
        "IPC message queues and communication log initialized",
        "OK"
    );
}

int enqueue_message(int sender, int receiver, const char *msg_type, const char *payload, int priority) {
    if (mq.count >= MAX_MESSAGES) {
        append_persistent_ipc_log(
            global_time,
            sender,
            receiver,
            "QUEUE_FULL",
            msg_type,
            payload,
            "FAILED"
        );
        LOG_WARNING("IPC", "Message queue full: P%d to P%d type=%s", sender, receiver, msg_type ? msg_type : "");
        return 0;
    }

    Message *msg = &mq.messages[mq.tail];
    msg->sender_pid = sender;
    msg->receiver_pid = receiver;
    safe_copy(msg->message_type, sizeof(msg->message_type), msg_type);
    safe_copy(msg->payload, sizeof(msg->payload), payload);
    msg->timestamp = global_time++;
    msg->priority = priority;

    mq.tail = (mq.tail + 1) % MAX_MESSAGES;
    mq.count++;

    append_persistent_ipc_log(
        msg->timestamp,
        sender,
        receiver,
        "ENQUEUE",
        msg->message_type,
        msg->payload,
        "QUEUED"
    );

    LOG_INFO(
        "IPC",
        "Queued %s message from P%d to P%d: %s",
        msg->message_type,
        sender,
        receiver,
        msg->payload
    );

    return 1;
}

int dequeue_message(int pid, Message *out_msg) {
    if (mq.count == 0) return 0;

    for (int i = 0; i < mq.count; i++) {
        int idx = (mq.head + i) % MAX_MESSAGES;
        if (mq.messages[idx].receiver_pid == pid) {
            *out_msg = mq.messages[idx];

            for (int j = i; j < mq.count - 1; j++) {
                int current = (mq.head + j) % MAX_MESSAGES;
                int next = (mq.head + j + 1) % MAX_MESSAGES;
                mq.messages[current] = mq.messages[next];
            }

            mq.tail = (mq.tail + MAX_MESSAGES - 1) % MAX_MESSAGES;
            mq.count--;

            append_persistent_ipc_log(
                global_time++,
                out_msg->sender_pid,
                out_msg->receiver_pid,
                "DEQUEUE",
                out_msg->message_type,
                out_msg->payload,
                "DELIVERED"
            );

            return 1;
        }
    }
    return 0;
}

int ipc_process_inbox(int pid, char *output, size_t output_size) {
    Message msg;
    int delivered = 0;

    while (dequeue_message(pid, &msg)) {
        delivered++;

        if (output && output_size > 0) {
            char line[512];
            snprintf(
                line,
                sizeof(line),
                "Time %-3d -> IPC delivered %s from P%d to P%d: %s\n",
                msg.timestamp,
                msg.message_type,
                msg.sender_pid,
                msg.receiver_pid,
                msg.payload
            );
            append_text(output, output_size, line);
        }

        LOG_INFO(
            "IPC",
            "Delivered %s message from P%d to P%d: %s",
            msg.message_type,
            msg.sender_pid,
            msg.receiver_pid,
            msg.payload
        );
    }

    return delivered;
}

void ipc_handle_scheduler_event(const char *event, int pid, const char *detail, int scheduler_time) {
    char payload[MAX_MESSAGE_SIZE];
    const char *message_type = "SCHED_EVT";

    snprintf(
        payload,
        sizeof(payload),
        "t=%d %s",
        scheduler_time,
        detail ? detail : ""
    );

    enqueue_message(IPC_SYSTEM_PID, pid, message_type, payload, 1);

    append_persistent_ipc_log(
        scheduler_time,
        IPC_SYSTEM_PID,
        pid,
        event,
        message_type,
        payload,
        "SCHEDULER"
    );

    if (event)
        update_coordinator_status(event);
}

void ipc_get_recent_logs(IpcLogEntry *out_logs, int max_count, int *count) {
    if (!out_logs || max_count <= 0 || !count) {
        if (count)
            *count = 0;
        return;
    }

    int copy_count = ipc_log_count < max_count ? ipc_log_count : max_count;
    int first = ipc_log_count - copy_count;

    for (int i = 0; i < copy_count; i++) {
        int src = (ipc_log_start + first + i) % MAX_IPC_LOGS;
        out_logs[i] = ipc_logs[src];
    }

    *count = copy_count;
}

void ipc_format_logs(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0)
        return;

    buffer[0] = '\0';

    char summary[512];
    snprintf(
        summary,
        sizeof(summary),
        "IPC STATUS REPORT\nQueued Messages: %d\nPipes: %d\nCoordinator: %s\n\nCOMMUNICATION LOG\n",
        mq.count,
        pipe_count,
        shared_mem.coordinator_status
    );
    append_text(buffer, buffer_size, summary);

    if (ipc_log_count == 0) {
        append_text(buffer, buffer_size, "No IPC activity recorded.\n");
        return;
    }

    IpcLogEntry logs[MAX_IPC_LOGS];
    int count = 0;
    ipc_get_recent_logs(logs, MAX_IPC_LOGS, &count);

    for (int i = 0; i < count; i++) {
        char sender[24];
        char receiver[24];
        char line[768];

        if (logs[i].sender_pid == IPC_SYSTEM_PID)
            strcpy(sender, "SYSTEM");
        else
            snprintf(sender, sizeof(sender), "P%d", logs[i].sender_pid);

        if (logs[i].receiver_pid == IPC_SYSTEM_PID)
            strcpy(receiver, "SYSTEM");
        else
            snprintf(receiver, sizeof(receiver), "P%d", logs[i].receiver_pid);

        snprintf(
            line,
            sizeof(line),
            "t=%-3d %-12s %-8s %s -> %s | %-10s | %s\n",
            logs[i].timestamp,
            logs[i].event,
            logs[i].message_type,
            sender,
            receiver,
            logs[i].status,
            logs[i].payload
        );
        append_text(buffer, buffer_size, line);
    }
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

    safe_copy(pipes[pipe_id].buffer, sizeof(pipes[pipe_id].buffer), data);
    pipes[pipe_id].has_data = 1;

    append_persistent_ipc_log(
        global_time++,
        pipes[pipe_id].producer_pid,
        pipes[pipe_id].consumer_pid,
        "PIPE_WRITE",
        "PIPE",
        data,
        "READY"
    );

    return 1;
}

int read_from_pipe(int pipe_id, char *data) {
    if (pipe_id < 0 || pipe_id >= pipe_count) return 0;

    if (!pipes[pipe_id].has_data) return 0;

    strcpy(data, pipes[pipe_id].buffer);
    pipes[pipe_id].has_data = 0;

    append_persistent_ipc_log(
        global_time++,
        pipes[pipe_id].producer_pid,
        pipes[pipe_id].consumer_pid,
        "PIPE_READ",
        "PIPE",
        data,
        "DELIVERED"
    );

    return 1;
}

SharedMemory* get_shared_memory() {
    return &shared_mem;
}

void update_coordinator_status(const char *status) {
    strncpy(shared_mem.coordinator_status, status,
            sizeof(shared_mem.coordinator_status) - 1);
    shared_mem.coordinator_status[sizeof(shared_mem.coordinator_status) - 1] = '\0';
}

const char* simulate_process_coordination(int initiator_pid, const char *emergency_type) {
    static char report[2048];
    memset(report, 0, sizeof(report));

    snprintf(report, sizeof(report), "IPC coordination started by P%d for %s\n", initiator_pid, emergency_type);

    enqueue_message(initiator_pid, 2, "ALERT", "Emergency coordination requested", 1);
    enqueue_message(initiator_pid, 3, "ALERT", "Emergency coordination requested", 1);

    int pipe_1 = create_pipe(initiator_pid, 2);
    write_to_pipe(pipe_1, "Shared incident data packet");

    update_coordinator_status("ACTIVE");

    return report;
}

const char* send_message(int sender, int receiver, const char *msg) {
    static char buffer[256];

    enqueue_message(sender, receiver, "DATA", msg, 2);

    snprintf(buffer, sizeof(buffer), "P%d -> P%d: %s", sender, receiver, msg);

    return buffer;
}

const char* get_ipc_report() {
    static char report[20000];
    ipc_format_logs(report, sizeof(report));
    return report;
}
