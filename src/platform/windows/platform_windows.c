#include "platform/platform.h"

void mcthread_init(void);
void mcthread_cleanup(void);

void platform_init(void) {
    mcthread_init();
}

void platform_cleanup(void) {
    mcthread_cleanup();
}