#include "platform/platform.h"
#ifdef MC_PLATFORM_LINUX

#include "signal-handler.h"

#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

void platform_init(void) {
    sigset_t global_sigmask;
    sigfillset(&global_sigmask);

    // Block the SIGINT & SIGTERM signals for all other threads.
    // Make sure the main thread is the one handling signals.
    pthread_sigmask(SIG_BLOCK, &global_sigmask, NULL);
    signal_system_init();
}

void platform_cleanup(void) {
    signal_system_cleanup();
}

const char* get_last_error(void) {
    return get_error_from_code(errno);
}

const char* get_error_from_code(i64 code) {
    return strerror(code);
}

#endif
