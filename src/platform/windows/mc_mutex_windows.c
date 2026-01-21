//
// Created by bmorino on 08/11/2024.
//

#ifdef MC_PLATFORM_WINDOWS

#include "platform/mc_mutex.h"

#include "containers/object_pool.h"

#include <synchapi.h>

bool mcmutex_create(MCMutex* mutex) {
    if (!mutex)
        return false;

    InitializeCriticalSection(mutex);

    return true;
}
bool mcmutex_destroy(MCMutex* mutex) {
    DeleteCriticalSection(mutex);
    return true;
}

bool mcmutex_lock(MCMutex* mutex) {
    EnterCriticalSection(mutex);
    return true;
}

bool mcmutex_trylock(MCMutex* mutex) {
    bool res = TryEnterCriticalSection(mutex);
    return res;
}
bool mcmutex_unlock(MCMutex* mutex) {
    LeaveCriticalSection(mutex);
    return true;
}

#endif
