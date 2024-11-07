#ifndef MC_THREADS_LINUX_H
#define MC_THREADS_LINUX_H

#include "definitions.h"
#include <pthread.h>

struct mc_thread {
    pthread_t handle;
    bool running;
};

typedef pthread_key_t MCThreadKey;

#endif /* ! MC_THREADS_LINUX_H */
