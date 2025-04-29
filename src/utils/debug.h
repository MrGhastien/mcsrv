#ifndef DEBUG_H
#define DEBUG_H

#include "definitions.h"
#include "logger.h"
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>

#define BACKTRACE_MAX_SIZE 64

[[noreturn]]
static inline void mc_abort(void) {
    #ifdef DEBUG
    log_fatal("ABORTED:");
    void* trace[BACKTRACE_MAX_SIZE];
    i32 res = backtrace(trace, BACKTRACE_MAX_SIZE);

    char** strings = backtrace_symbols(trace, res);
    for (i32 i = 0; i < res; i++) {
        log_fatalf("  at %s", strings[i]);
    }
    #endif
    abort();
}

#endif /* ! DEBUG_H */
