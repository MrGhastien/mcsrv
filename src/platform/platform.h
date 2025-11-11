/**
 * Standard interface for platform-specific operations and handles.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "definitions.h"
#include "mem_advice.h"

/**
 * Initializes platform-specific sub-systems.
 */
void platform_init(void);
void platform_cleanup(void);

const char* get_last_error(void);
const char* get_error_from_code(i64 code);

/**
 * Allocates memory using the plaform's APIs.
 *
 * @param[inout] capacity A pointer to a @ref u64 object containing the requested size of the
 * allocation. The final size is written to it after the allocation.
 * @param[in] advice A hint about how the memory will be used.
 * @return The address of the allocated memory, or @p NULL on failure.
 */
void* platform_alloc(u64* capacity, enum MemoryAdvice advice);
void platform_mem_advise(void* mem, u64 size, enum MemoryAdvice advice);
void platform_free(void* ptr, u64 size);

void platform_abort(void);

#define platform_assert(cond, msg)                                                                 \
    if (!(cond)) {                                                                                 \
        log_fatalf("Assertion '%s' failed: %s", #cond, msg);                                       \
        platform_abort();                                                                          \
    }

#endif /* ! PLATFORM_H */
