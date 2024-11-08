#include "platform/mc_mutex.h"

#ifdef MC_PLATFORM_LINUX
#include "mc_mutex.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>

bool mcmutex_create(MCMutex* mutex) {
    pthread_mutex_init(&mutex->internal_lock, NULL);
    return TRUE;
}

bool mcmutex_destroy(MCMutex *mutex) {
    i32 res = pthread_mutex_destroy(&mutex->internal_lock);
    if(res) {
        log_errorf("Failed to destroy mutex: %s.", strerror(res));
        return FALSE;
    }

    return TRUE;
}

bool mcmutex_lock(MCMutex *mutex) {
    i32 res = pthread_mutex_lock(&mutex->internal_lock);
    if(res) {
        log_errorf("Unable to aqcuire MutEx lock: %s.", strerror(res));
        return FALSE;
    }
    return TRUE;
}

bool mcmutex_unlock(MCMutex *mutex) {
    i32 res = pthread_mutex_unlock(&mutex->internal_lock);
    if(res) {
        log_errorf("Failed to release MutEx lock: %s", strerror(res));
        return FALSE;
    }
    return TRUE;
}

#endif
