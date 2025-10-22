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

#ifdef DEBUG
#include <elfutils/libdwfl.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define BACKTRACE_MAX_SIZE 64
#endif

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

#ifdef DEBUG
// https://gist.github.com/fclairamb/4bda5020d2ea78966953 thanks
static void print_debug_info(const char* unw_function_name, u64 offset, const u64 ip) {
    char* debuginfo_path = NULL;

    Dwfl_Callbacks callbacks = {
        .find_elf       = dwfl_linux_proc_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
        .debuginfo_path = &debuginfo_path,
    };

    Dwfl* dwfl = dwfl_begin(&callbacks);
    dwfl_linux_proc_report(dwfl, getpid());
    dwfl_report_end(dwfl, NULL, NULL);

    Dwarf_Addr addr = ip;
    // Dwfl_Module* module = dwfl_addrmodule(dwfl, addr);

    // const char* function_name = dwfl_module_addrname(module, addr);

    Dwfl_Line* line = dwfl_getsrc(dwfl, addr);
    if (line != NULL) {
        i32 nline;
        Dwarf_Addr addr;
        const char* filename = dwfl_lineinfo(line, &addr, &nline, NULL, NULL, NULL);
        log_fatalf("  at %s (%s:%i)", unw_function_name, strrchr(filename, '/') + 1, nline);
    } else {
        log_fatalf("  at %s<+%li> [%p]", unw_function_name, offset, ip);
    }
}
#endif

inline void platform_abort(void) {
#ifdef DEBUG
    log_fatal("ABORTED:");
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, sp, offp;

    char buf[1024];

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    while (unw_step(&cursor) > 0) {
        unw_get_proc_name(&cursor, buf, 1024, &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        if (!buf[0]) {
            buf[0] = '?';
            buf[1] = 0;
        }
        // log_fatalf("  at [0x%lx] %s <+%li>", (i64) ip, buf, (i64) offp);
        print_debug_info(buf, offp, (u64) ip);
    }
#endif
    abort();
}

#endif
