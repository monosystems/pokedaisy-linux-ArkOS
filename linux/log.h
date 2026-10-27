// A tiny log: stderr always, plus a file when the port script asks for one.
// On the handheld this file is the only way to see what happened.
#ifndef PD_LOG_H
#define PD_LOG_H

#include <stdbool.h>

// Opens path for appending (NULL or "" = stderr only). False when it can't.
bool pd_log_open(const char* path);
void pd_log_close(void);
void pd_logf(const char* fmt, ...);

#endif
