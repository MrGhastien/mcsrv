#ifndef MC_MUTEX_H
#define MC_MUTEX_H

#include "definitions.h"

#ifdef MC_PLATFORM_LINUX
#include "mc_mutex_linux.h"
#endif

typedef struct mc_mutex MCMutex;

bool mcmutex_create(MCMutex* mutex);
bool mcmutex_destroy(MCMutex* mutex);

bool mcmutex_lock(MCMutex* mutex);
bool mcmutex_unlock(MCMutex* mutex);

#endif /* ! MC_MUTEX_H */
