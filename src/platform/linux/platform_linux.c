#include "logger.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include <stdlib.h>
#include <unistd.h>
#ifdef MC_PLATFORM_LINUX

#include "memory/_memory_internal.h"
#include "platform/platform.h"
#include "signal-handler.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

static u64 page_size;

void platform_init(void) {
    sigset_t global_sigmask;
    sigfillset(&global_sigmask);

    // Block the SIGINT & SIGTERM signals for all other threads.
    // Make sure the main thread is the one handling signals.
    pthread_sigmask(SIG_BLOCK, &global_sigmask, NULL);
    page_size = sysconf(_SC_PAGESIZE);
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

void* platform_alloc(u64* capacity) {
    void* ptr = mmap(NULL, *capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        log_fatalf("Memory allocation failed: %s", get_last_error());
        return NULL;
    }

    *capacity = ceil_u64(*capacity, page_size);
    return ptr;
}
void platform_free(void* ptr, u64 size) {
    if (munmap(ptr, size) != 0)
        log_fatalf("Memory de-allocation failed: %s", get_last_error());
}

#endif
