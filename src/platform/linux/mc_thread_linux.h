#ifndef MC_THREADS_LINUX_H
#define MC_THREADS_LINUX_H

#include "definitions.h"
#include <pthread.h>

struct MCThread {
    pthread_t handle;
};

typedef pthread_key_t MCThreadKey;

#endif /* ! MC_THREADS_LINUX_H */
