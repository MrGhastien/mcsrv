#ifdef MC_PLATFORM_WINDOWS

#include "platform/platform.h"
#include "definitions.h"

#include <windows.h>

void mcthread_init(void);
void mcthread_cleanup(void);

void platform_init(void) {
    mcthread_init();
}

void platform_cleanup(void) {
    mcthread_cleanup();
}

const char* get_last_error(void) {
    return get_error_from_code(GetLastError());
}

const char* get_error_from_code(i64 code) {
    static char error_msg[2048];
    FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  code,
                  MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_DEFAULT),
                  error_msg,
                  2048,
                  NULL);
    return error_msg;

}


#endif
