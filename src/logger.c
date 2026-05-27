#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "logger.h"

#define MAX_LOG_ENTRIES 1000
#define LOGGER_FILENAME "system_log.txt"

static FILE *log_file = NULL;
static LogEntry log_buffer[MAX_LOG_ENTRIES];
static int log_count = 0;

const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARNING: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void logger_init(const char *filename) {
    log_file = fopen(filename, "a+");
    if (log_file) {
        setvbuf(log_file, NULL, _IONBF, 0);
    }
    log_count = 0;
}

void logger_log(LogLevel level, const char *component, const char *format, ...) {
    if (!log_file) return;

    if (log_count >= MAX_LOG_ENTRIES) return;

    va_list args;
    va_start(args, format);

    LogEntry *entry = &log_buffer[log_count++];

    entry->timestamp = time(NULL);
    entry->level = level;

    strncpy(entry->component, component, sizeof(entry->component) - 1);
    entry->component[sizeof(entry->component) - 1] = '\0';
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    entry->message[sizeof(entry->message) - 1] = '\0';

    va_end(args);

    fprintf(
        log_file,
        "%lld\t%d\t%s\t%s\n",
        (long long)entry->timestamp,
        entry->level,
        entry->component,
        entry->message
    );
}

void logger_shutdown() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void logger_clear_logs(void) {
    logger_shutdown();
    remove(LOGGER_FILENAME);
    logger_init(LOGGER_FILENAME);
}

void logger_get_recent_logs(LogEntry *out_logs, int max_count, int *count) {
    if (!out_logs || max_count <= 0 || !count) {
        if (count) *count = 0;
        return;
    }

    *count = 0;

    FILE *f = fopen(LOGGER_FILENAME, "r");
    if (!f)
        return;

    LogEntry *buffer = malloc(sizeof(LogEntry) * max_count);
    if (!buffer) {
        fclose(f);
        return;
    }

    char line[1024];
    int index = 0;
    int total = 0;

    while (fgets(line, sizeof(line), f)) {
        char *token = strtok(line, "\t");
        if (!token) continue;
        buffer[index].timestamp = (time_t)atoll(token);

        token = strtok(NULL, "\t");
        if (!token) continue;
        buffer[index].level = atoi(token);

        token = strtok(NULL, "\t");
        if (!token) continue;
        strncpy(buffer[index].component, token, sizeof(buffer[index].component) - 1);
        buffer[index].component[sizeof(buffer[index].component) - 1] = '\0';

        token = strtok(NULL, "\n");
        if (!token) token = "";
        strncpy(buffer[index].message, token, sizeof(buffer[index].message) - 1);
        buffer[index].message[sizeof(buffer[index].message) - 1] = '\0';

        index = (index + 1) % max_count;
        total++;
    }

    fclose(f);

    int start = total > max_count ? index : 0;
    int count_to_copy = total > max_count ? max_count : total;

    for (int i = 0; i < count_to_copy; i++) {
        int src = (start + i) % max_count;
        out_logs[i] = buffer[src];
    }

    *count = count_to_copy;
    free(buffer);
}