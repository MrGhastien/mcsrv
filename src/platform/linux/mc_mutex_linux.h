#ifndef MC_MUTEX_LINUX_H
#define MC_MUTEX_LINUX_H

#include <pthread.h>

struct mc_mutex {
    pthread_mutex_t internal_lock;
};

#endif /* ! MC_MUTEX_LINUX_H */
