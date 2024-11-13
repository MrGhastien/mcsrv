//
// Created by bmorino on 08/11/2024.
//

#include "platform/mc_mutex.h"

#include <containers/object_pool.h>
#include <memory/arena.h>
#include <synchapi.h>

bool mcmutex_create(MCMutex* mutex) {
    if(!mutex)
        return FALSE;

   InitializeCriticalSection(mutex);

    return TRUE;
}
bool mcmutex_destroy(MCMutex* mutex) {
    DeleteCriticalSection(mutex);
    return TRUE;
}

bool mcmutex_lock(MCMutex* mutex) {
    EnterCriticalSection(mutex);
    return TRUE;
}

bool mcmutex_trylock(MCMutex* mutex) {
    bool res = TryEnterCriticalSection(mutex);
    return res;
}
bool mcmutex_unlock(MCMutex* mutex) {
    LeaveCriticalSection(mutex);
    return TRUE;
}
