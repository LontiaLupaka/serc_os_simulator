#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_CRITICAL = 4
} LogLevel;

typedef struct {
    time_t timestamp;
    LogLevel level;
    char component[32];
    char message[512];
} LogEntry;

/* Logging Functions */
void logger_init(const char *filename);
void logger_log(LogLevel level, const char *component, const char *format, ...);
void logger_shutdown();
void logger_clear_logs(void);
void logger_get_recent_logs(LogEntry *out_logs, int max_count, int *count);

/* Convenience macros */
#define LOG_DEBUG(comp, fmt, ...) logger_log(LOG_LEVEL_DEBUG, comp, fmt, ##__VA_ARGS__)
#define LOG_INFO(comp, fmt, ...) logger_log(LOG_LEVEL_INFO, comp, fmt, ##__VA_ARGS__)
#define LOG_WARNING(comp, fmt, ...) logger_log(LOG_LEVEL_WARNING, comp, fmt, ##__VA_ARGS__)
#define LOG_ERROR(comp, fmt, ...) logger_log(LOG_LEVEL_ERROR, comp, fmt, ##__VA_ARGS__)
#define LOG_CRITICAL(comp, fmt, ...) logger_log(LOG_LEVEL_CRITICAL, comp, fmt, ##__VA_ARGS__)

#endif