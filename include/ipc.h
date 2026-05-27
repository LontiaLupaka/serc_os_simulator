#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "config.h"

/* IPC Mechanisms */
#define MAX_MESSAGES 100
#define MAX_MESSAGE_SIZE 256
#define MAX_PIPES 25
#define MAX_IPC_LOGS 200
#define IPC_SYSTEM_PID 0
#define IPC_LOG_SAVE_FILE "ipc_history.dat"

/* Message Queue Implementation */
typedef struct {
    int sender_pid;
    int receiver_pid;
    char message_type[32];  /* "ALERT", "ACK", "DATA", "RESOURCE_REQ" */
    char payload[MAX_MESSAGE_SIZE];
    int timestamp;
    int priority;
} Message;

typedef struct {
    Message messages[MAX_MESSAGES];
    int head;
    int tail;
    int count;
} MessageQueue;

typedef struct {
    int timestamp;
    int sender_pid;
    int receiver_pid;
    char event[32];
    char message_type[32];
    char payload[MAX_MESSAGE_SIZE];
    char status[32];
} IpcLogEntry;

/* Pipe Implementation */
typedef struct {
    int producer_pid;
    int consumer_pid;
    char buffer[MAX_MESSAGE_SIZE];
    int has_data;
} Pipe;

/* Shared Memory Implementation */
typedef struct {
    int process_ids[MAX_PROCESSES];
    int process_count;
    int total_memory_used;
    int active_processes;
    char coordinator_status[128];
} SharedMemory;

/* Function Declarations */
const char* send_message(int sender, int receiver, const char *msg);
void init_ipc_system();
int enqueue_message(int sender, int receiver, const char *msg_type, const char *payload, int priority);
int dequeue_message(int pid, Message *out_msg);
int ipc_process_inbox(int pid, char *output, size_t output_size);
void ipc_handle_scheduler_event(const char *event, int pid, const char *detail, int scheduler_time);
void ipc_get_recent_logs(IpcLogEntry *out_logs, int max_count, int *count);
void ipc_format_logs(char *buffer, size_t buffer_size);
int create_pipe(int producer_pid, int consumer_pid);
int write_to_pipe(int pipe_id, const char *data);
int read_from_pipe(int pipe_id, char *data);
SharedMemory* get_shared_memory();
void update_coordinator_status(const char *status);
const char* get_ipc_report();
const char* simulate_process_coordination(int severity, const char *emergency_type);

#endif
