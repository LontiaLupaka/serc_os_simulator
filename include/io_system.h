#ifndef IO_SYSTEM_H
#define IO_SYSTEM_H

/* ================= I/O CONFIG ================= */

/* Default I/O operation times (can be tuned) */
#define IO_READ_TIME     2
#define IO_WRITE_TIME    3
#define IO_DISK_TIME     5
#define IO_NETWORK_TIME  4

/* ================= I/O TYPES ================= */

typedef enum {
    IO_READ = 0,
    IO_WRITE,
    IO_DISK,
    IO_NETWORK
} IOType;

/* ================= I/O STRUCT ================= */

typedef struct {
    int process_id;
    IOType type;
    int remaining_time;
} IOOperation;

/* ================= FUNCTION DECLARATIONS ================= */

void create_io_operation(int process_id, IOType type);
void process_io_queue();

#endif