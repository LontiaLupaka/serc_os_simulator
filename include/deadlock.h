#ifndef DEADLOCK_H
#define DEADLOCK_H

#include "process.h"

/* Banker's Algorithm for Deadlock Prevention */

/* Resource types */
#define RESOURCE_MEMORY 0
#define RESOURCE_VEHICLE 1
#define RESOURCE_CHANNEL 2
#define NUM_RESOURCES 3

/* Backward-compatible names used by older process code */
#define RESOURCE_CPU RESOURCE_VEHICLE
#define RESOURCE_IO RESOURCE_CHANNEL

/* Resource allocation tracking */
typedef struct {
    int allocated[NUM_RESOURCES];    /* Resources currently allocated */
    int maximum[NUM_RESOURCES];      /* Max resources this process can need */
    int need[NUM_RESOURCES];         /* Resources still needed */
} ResourceAllocation;

extern ResourceAllocation resource_state[MAX_PROCESSES];
extern int total_resources[NUM_RESOURCES];
extern int available_resources[NUM_RESOURCES];

/* Functions */
const char* detect_deadlock();
int can_allocate_resources(int process_index, int memory, int vehicles, int channels);
void register_process_resources(int process_index, int memory, int vehicles, int channels);
int request_process_resources(int process_index);
int process_holds_resources(int process_index);
void allocate_resources(int process_index, int memory, int vehicles, int channels);
void release_resources(int process_index);
void init_deadlock_detection();

#endif
