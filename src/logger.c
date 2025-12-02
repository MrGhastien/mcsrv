
#ifndef MC_CPLX_LOGGER

#include "logger.h"
#include "platform/mc_mutex.h"
#include "utils/ansi_codes.h"

#include <stdarg.h>
#include <stdio.h>

#define LOGGER_ARENA_SIZE 262144

static char* names[_LOG_LEVEL_COUNT] = {
  [LOG_LEVEL_FATAL] = "\x1b[91;7m FATAL " ANSI_RESET,
  [LOG_LEVEL_ERROR] = "\x1b[31;7m ERROR " ANSI_RESET,
  [LOG_LEVEL_WARN]  = "\x1b[33;7m  WARN " ANSI_RESET,
  [LOG_LEVEL_INFO]  = "\x1b[0;7m  INFO " ANSI_RESET,
#ifdef DEBUG
    [LOG_LEVEL_DEBUG] = "\x1b[32;3;7m DEBUG " ANSI_RESET,
#endif
#ifdef TRACE
    [LOG_LEVEL_TRACE] = "\x1b[30;3;7m TRACE " ANSI_RESET,
#endif
};

static char* colors[_LOG_LEVEL_COUNT] = {
  [LOG_LEVEL_FATAL] = "\x1b[91m",
    [LOG_LEVEL_ERROR] = "\x1b[31m",
    [LOG_LEVEL_WARN]  = "\x1b[33m",
    [LOG_LEVEL_INFO]  = "\x1b[0m",
#ifdef DEBUG
    [LOG_LEVEL_DEBUG] = "\x1b[32;3m",
#endif
#ifdef TRACE
    [LOG_LEVEL_TRACE] = "\x1b[30;3m",
#endif
};

typedef struct logger_ctx {
    MCMutex mutex;
} LoggerCtx;

static LoggerCtx ctx;

void logger_system_init(void) {
    mcmutex_create(&ctx.mutex);
}

void logger_system_cleanup(void) {
    mcmutex_destroy(&ctx.mutex);
}

void _log_msg(enum LogLevel lvl, const char* msg) {
    if (lvl < LOG_LEVEL_FATAL || lvl >= _LOG_LEVEL_COUNT)
        return;

    FILE* stream;
    switch (lvl) {
    case LOG_LEVEL_FATAL:
    case LOG_LEVEL_ERROR:
        stream = stderr;
        break;
    default:
        stream = stdout;
        break;
    }

    fprintf(stream, "%s%s %s" ANSI_RESET "\n", names[lvl], colors[lvl], msg);
}

void _log_msgf(enum LogLevel lvl, const char* msg, ...) {
    if (lvl < LOG_LEVEL_FATAL || lvl >= _LOG_LEVEL_COUNT)
        return;

    va_list args;
    va_start(args, msg);

    FILE* stream;
    switch (lvl) {
    case LOG_LEVEL_FATAL:
    case LOG_LEVEL_ERROR:
        stream = stderr;
        break;
    default:
        stream = stdout;
        break;
    }

    mcmutex_lock(&ctx.mutex);

    fprintf(stream, "%s%s ", names[lvl], colors[lvl]);
    vfprintf(stream, msg, args);
    puts(ANSI_RESET);

    mcmutex_unlock(&ctx.mutex);
    va_end(args);
}

#endif
