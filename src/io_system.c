#include <stdio.h>
#include "../include/io_system.h"

/* ================= GLOBAL I/O QUEUE ================= */

#define MAX_IO 100

static IOOperation io_queue[MAX_IO];
static int io_count = 0;

/* ================= CREATE I/O ================= */

void create_io_operation(int process_id, IOType type) {

    if (io_count >= MAX_IO) {
        printf("I/O Queue Full!\n");
        return;
    }

    int duration = 0;

    switch(type) {
        case IO_READ:
            duration = IO_READ_TIME;
            break;

        case IO_WRITE:
            duration = IO_WRITE_TIME;
            break;

        case IO_DISK:
            duration = IO_DISK_TIME;
            break;

        case IO_NETWORK:
            duration = IO_NETWORK_TIME;
            break;

        default:
            duration = 1; // safety fallback
            break;
    }

    io_queue[io_count].process_id = process_id;
    io_queue[io_count].type = type;
    io_queue[io_count].remaining_time = duration;

    io_count++;
}

/* ================= PROCESS I/O ================= */

void process_io_queue() {

    for (int i = 0; i < io_count; i++) {

        if (io_queue[i].remaining_time > 0) {
            io_queue[i].remaining_time--;

            if (io_queue[i].remaining_time == 0) {
                printf("I/O complete for Process %d\n", io_queue[i].process_id);
            }
        }
    }
}