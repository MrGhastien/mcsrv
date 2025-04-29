/**
 * Standard interface for platform-specific operations and handles.
*/

#ifndef PLATFORM_H
#define PLATFORM_H

#include "definitions.h"

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
 * @param[inout] capacity A pointer to a @ref u64 object containing the requested size of the allocation. The final size is written to it after the allocation.
 * @return The address of the allocated memory, or @p NULL on failure.
 */
void* platform_alloc(u64* capacity);
void platform_free(void* ptr, u64 size);

#endif /* ! PLATFORM_H */
