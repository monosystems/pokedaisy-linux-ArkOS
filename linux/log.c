#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE* logFile;

bool pd_log_open(const char* path) {
    if (!path || !*path) {
        return true;
    }
    logFile = fopen(path, "a");
    return logFile != NULL;
}

void pd_log_close(void) {
    if (logFile) {
        fclose(logFile);
        logFile = NULL;
    }
}

void pd_logf(const char* fmt, ...) {
    char line[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    char stamp[16] = "--:--:--";
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    if (tm) {
        strftime(stamp, sizeof(stamp), "%H:%M:%S", tm);
    }
    fprintf(stderr, "[%s] %s\n", stamp, line);
    if (logFile) {
        fprintf(logFile, "[%s] %s\n", stamp, line);
        fflush(logFile);
    }
}
