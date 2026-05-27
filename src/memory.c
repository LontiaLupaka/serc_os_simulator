#include <stdio.h>
#include <string.h>

#include "../include/memory.h"
#include "../include/logger.h"

/* =====================================================
   GLOBAL MEMORY TRACKER
===================================================== */

#define MAX_MEMORY_BLOCKS ((MAX_PROCESSES * 2) + 1)

typedef struct {
    int start;
    int size;
    int allocated;
} MemoryBlock;

static MemoryBlock blocks[MAX_MEMORY_BLOCKS];
static int block_count = 0;
static int used = 0;
static MemoryAllocationStrategy current_strategy = MEMORY_FIRST_FIT;

MemorySystem memory_system = {0};

static void update_memory_statistics() {
    memory_system.total_free_pages =
        MAX_PAGES -
        ((used * MAX_PAGES) / TOTAL_MEMORY);

    if (memory_system.total_free_pages < 0)
        memory_system.total_free_pages = 0;

    if (memory_system.total_free_pages > MAX_PAGES)
        memory_system.total_free_pages = MAX_PAGES;
}

static void reset_memory_blocks() {
    memset(blocks, 0, sizeof(blocks));

    block_count = 1;
    blocks[0].start = 0;
    blocks[0].size = TOTAL_MEMORY;
    blocks[0].allocated = 0;
}

static int find_block_for_strategy(int size) {
    int selected = -1;

    for (int i = 0; i < block_count; i++) {
        if (blocks[i].allocated || blocks[i].size < size)
            continue;

        if (current_strategy == MEMORY_FIRST_FIT)
            return i;

        if (selected == -1) {
            selected = i;
            continue;
        }

        if (current_strategy == MEMORY_BEST_FIT &&
            blocks[i].size < blocks[selected].size) {
            selected = i;
        }
        else if (current_strategy == MEMORY_WORST_FIT &&
                 blocks[i].size > blocks[selected].size) {
            selected = i;
        }
    }

    return selected;
}

static void merge_free_blocks() {
    int i = 0;

    while (i < block_count - 1) {
        if (!blocks[i].allocated && !blocks[i + 1].allocated) {
            blocks[i].size += blocks[i + 1].size;

            for (int j = i + 1; j < block_count - 1; j++)
                blocks[j] = blocks[j + 1];

            block_count--;
            continue;
        }

        i++;
    }
}

static int largest_free_block() {
    int largest = 0;

    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].allocated && blocks[i].size > largest)
            largest = blocks[i].size;
    }

    return largest;
}

static int total_free_block_memory() {
    int free_memory = 0;

    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].allocated)
            free_memory += blocks[i].size;
    }

    return free_memory;
}

void set_memory_allocation_strategy(MemoryAllocationStrategy strategy) {
    if (strategy < MEMORY_FIRST_FIT || strategy > MEMORY_WORST_FIT)
        return;

    current_strategy = strategy;
}

MemoryAllocationStrategy get_memory_allocation_strategy() {
    return current_strategy;
}

const char *get_memory_allocation_strategy_name() {
    switch (current_strategy) {
        case MEMORY_BEST_FIT:
            return "Best Fit";
        case MEMORY_WORST_FIT:
            return "Worst Fit";
        case MEMORY_FIRST_FIT:
        default:
            return "First Fit";
    }
}

/* =====================================================
   INITIALIZE MEMORY SYSTEM
===================================================== */

void initialize_memory_system() {

    /*
       IMPORTANT:
       This function must ONLY run once
       when the application starts.
    */

    printf("MEMORY SYSTEM INITIALIZED\n");

    memory_system.total_pages = MAX_PAGES;

    memory_system.total_free_pages = MAX_PAGES;

    memory_system.page_faults = 0;

    memory_system.fragmentation_percentage = 0;

    used = 0;

    current_strategy = MEMORY_FIRST_FIT;

    reset_memory_blocks();
}

/* =====================================================
   ALLOCATE MEMORY
===================================================== */

int allocate_memory(int size) {

    if (size <= 0)
        return 0;

    int block_index = find_block_for_strategy(size);

    if (block_index < 0) {

        printf(
            "MEMORY ALLOCATION FAILED (%d requested, %d free)\n",
            size,
            get_memory_free()
        );

        return 0;
    }

    if (blocks[block_index].size > size) {
        if (block_count >= MAX_MEMORY_BLOCKS) {
            printf("MEMORY ALLOCATION FAILED (memory map full)\n");
            return 0;
        }

        for (int i = block_count; i > block_index + 1; i--)
            blocks[i] = blocks[i - 1];

        blocks[block_index + 1].start =
            blocks[block_index].start + size;
        blocks[block_index + 1].size =
            blocks[block_index].size - size;
        blocks[block_index + 1].allocated = 0;

        blocks[block_index].size = size;
        block_count++;
    }

    blocks[block_index].allocated = 1;
    used += size;

    update_memory_statistics();

    printf(
        "MEMORY ALLOCATED: %d MB | STRATEGY: %s | TOTAL USED: %d/%d\n",
        size,
        get_memory_allocation_strategy_name(),
        used,
        TOTAL_MEMORY
    );

    return 1;
}

/* =====================================================
   DEALLOCATE MEMORY
===================================================== */

int deallocate_memory(int size) {

    if (size <= 0)
        return 0;

    int block_index = -1;

    for (int i = 0; i < block_count; i++) {
        if (blocks[i].allocated && blocks[i].size == size) {
            block_index = i;
            break;
        }
    }

    if (block_index < 0) {
        for (int i = 0; i < block_count; i++) {
            if (blocks[i].allocated && blocks[i].size >= size) {
                block_index = i;
                break;
            }
        }
    }

    if (block_index < 0)
        return 0;

    used -= blocks[block_index].size;

    if (used < 0)
        used = 0;

    blocks[block_index].allocated = 0;
    merge_free_blocks();
    update_memory_statistics();

    printf(
        "MEMORY FREED: %d MB | TOTAL USED: %d/%d\n",
        size,
        used,
        TOTAL_MEMORY
    );

    return 1;
}

/* =====================================================
   GET MEMORY USED
===================================================== */

int get_memory_used() {

    return used;
}

/* =====================================================
   GET MEMORY FREE
===================================================== */

int get_memory_free() {

    return total_free_block_memory();
}

/* =====================================================
   PAGE ALLOCATION (SIMULATION)
===================================================== */

int allocate_page(int pid) {

    (void)pid;

    /*
       Placeholder for future paging system
    */

    return -1;
}

/* =====================================================
   PAGE DEALLOCATION
===================================================== */

int deallocate_page(int pid, int page_number) {

    (void)pid;
    (void)page_number;

    return 0;
}

/* =====================================================
   FRAGMENTATION CALCULATION
===================================================== */

int calculate_fragmentation() {

    if (used == 0)
        return 0;

    int free_memory = total_free_block_memory();

    if (free_memory == 0)
        return 0;

    int fragmentation =
        ((free_memory - largest_free_block()) * 100) / free_memory;

    memory_system.fragmentation_percentage =
        fragmentation;

    return fragmentation;
}

void reset_memory_system() {
    used = 0;
    current_strategy = MEMORY_FIRST_FIT;
    reset_memory_blocks();
    memory_system.total_pages = MAX_PAGES;
    memory_system.total_free_pages = MAX_PAGES;
    memory_system.page_faults = 0;
    memory_system.fragmentation_percentage = 0;
}

/* =====================================================
   PRINT MEMORY STATUS
===================================================== */

void print_memory_status(char *buffer) {

    char temp[512];

    sprintf(
        temp,
        "=========== MEMORY STATUS ===========\n"
        "ALLOCATION STRATEGY: %s\n"
        "USED MEMORY : %d MB\n"
        "FREE MEMORY : %d MB\n"
        "TOTAL MEMORY: %d MB\n"
        "LARGEST FREE BLOCK: %d MB\n"
        "FRAGMENTATION: %d%%\n"
        "=====================================\n",

        get_memory_allocation_strategy_name(),
        used,
        get_memory_free(),
        TOTAL_MEMORY,
        largest_free_block(),
        calculate_fragmentation()
    );

    strcat(buffer, temp);
}
