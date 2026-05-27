#ifndef CONFIG_H
#define CONFIG_H

/* ================= SYSTEM LIMITS ================= */

#define MAX_PROCESSES 50
#define MAX_PAGES 256

#define TOTAL_MEMORY 1024
#define TOTAL_CPU_UNITS 100
#define TOTAL_IO_UNITS 50

#define DEFAULT_TIME_QUANTUM 3

/* ================= PRIORITY LEVELS ================= */
/* IMPORTANT: LOWER VALUE = HIGHER PRIORITY */

#define PRIORITY_CRITICAL 1
#define PRIORITY_HIGH     2
#define PRIORITY_NORMAL   3

/* ================= PROCESS STATES ================= */

#define PROCESS_NEW              0
#define PROCESS_READY            1
#define PROCESS_RUNNING          2
#define PROCESS_WAITING_IO       3
#define PROCESS_WAITING_RESOURCE 4
#define PROCESS_TERMINATED       5

/* ================= SCHEDULER TYPES ================= */

#define SCHEDULER_PRIORITY     1
#define SCHEDULER_ROUND_ROBIN  2
#define SCHEDULER_HYBRID       3

/* ================= SCORING THRESHOLDS ================= */

#define CRITICAL_SCORE_THRESHOLD 80
#define HIGH_SCORE_THRESHOLD     50

/* ================= MEMORY SAFETY ================= */

#define MIN_MEMORY_SAFETY_MARGIN 50

/* ================= SYSTEM CONFIG STRUCT ================= */

typedef struct {
    int total_memory;
    int total_cpu_units;
    int total_io_units;
    int num_cpu_cores;
    int time_quantum;
    int enable_io_simulation;
    int enable_starvation_detection;
    int enable_logging;
    const char *log_file;
} SystemConfig;

/* ================= GLOBAL CONFIG ================= */

extern SystemConfig system_config;

/* ================= FUNCTIONS ================= */

void load_system_config();
void print_system_config();

#endif