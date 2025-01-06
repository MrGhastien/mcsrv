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

#endif /* ! PLATFORM_H */
