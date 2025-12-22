#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

#ifndef LOGLVL
#define LOGLVL 0
#endif

#define _LOGF(lvl, fmt, ...) do { \
    FILE *log_file = fopen("log.txt", "a"); \
    if (log_file) { \
        fprintf(log_file, "[%s] | [%s:%s:%d] " fmt "\n", \
            lvl, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        fclose(log_file); \
    } \
} while(0);
#define _LOG(lvl, x) _LOGF(lvl, "%s", x)

#if LOGLVL >= 1
#define LERR(x) _LOG("ERR", x)
#define LERRF(fmt, ...) _LOGF("ERR", fmt, ##__VA_ARGS__)
#else
#define LERR(x)
#define LERRF(fmt, ...)
#endif

#if LOGLVL >= 2
#define LWARN(x) _LOG("WRN", x)
#define LWARNF(fmt, ...) _LOGF("WRN", fmt, ##__VA_ARGS__)
#else
#define LWARN(x)
#define LWARNF(fmt, ...)
#endif

#if LOGLVL >= 3
#define LDBG(x) _LOG("DBG", x)
#define LDBGF(fmt, ...) _LOGF("DBG", fmt, ##__VA_ARGS__)
#else
#define LDBG(x)
#define LDBGF(fmt, ...)
#endif

#endif