#ifndef MC_MUTEX_H
#define MC_MUTEX_H

#include "definitions.h"

#ifdef MC_PLATFORM_LINUX
#include "linux/mc_mutex_linux.h"
#elif defined MC_PLATFORM_WINDOWS
#include "windows/mc_mutex_windows.h"
#endif

typedef struct MCMutex MCMutex;

bool mcmutex_create(MCMutex* mutex);
bool mcmutex_destroy(MCMutex* mutex);

bool mcmutex_lock(MCMutex* mutex);
bool mcmutex_unlock(MCMutex* mutex);

#endif /* ! MC_MUTEX_H */
