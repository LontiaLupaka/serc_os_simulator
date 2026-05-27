#ifndef MEMORY_H
#define MEMORY_H

#include "config.h"

#define PAGE_FRAME_INVALID -1

/* Memory allocation status */
typedef struct {
    int allocated;
    int pid;
    int page_number;
} MemoryFrame;

typedef struct {
    MemoryFrame frames[MAX_PAGES];
    int total_pages;
    int total_free_pages;
    int page_faults;
    int fragmentation_percentage;
} MemorySystem;

typedef enum {
    MEMORY_FIRST_FIT = 0,
    MEMORY_BEST_FIT,
    MEMORY_WORST_FIT
} MemoryAllocationStrategy;

extern MemorySystem memory_system;

/* Memory Management Functions */
int allocate_memory(int size);
int deallocate_memory(int size);
int get_memory_used();
int get_memory_free();
void set_memory_allocation_strategy(MemoryAllocationStrategy strategy);
MemoryAllocationStrategy get_memory_allocation_strategy();
const char *get_memory_allocation_strategy_name();
int allocate_page(int pid);
int deallocate_page(int pid, int page_number);
int calculate_fragmentation();
void initialize_memory_system();
void reset_memory_system();
void print_memory_status(char *buffer);

#endif
