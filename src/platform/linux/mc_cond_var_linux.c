#ifdef MC_PLATFORM_LINUX

#include "platform/mc_cond_var.h"

#include "logger.h"

#include <errno.h>

bool mcvar_create(MCCondVar* cond_var) {
    pthread_cond_init(cond_var, NULL);
    return TRUE;
}

bool mcvar_destroy(MCCondVar* cond_var) {
    i32 res = pthread_cond_destroy(cond_var);
    switch (res) {
    case EBUSY:
        log_error(
            "Could not destroy condition variable, other threads are currently waiting on it.");
        return FALSE;
    default:
        return TRUE;
    }
}

void mcvar_wait(MCCondVar* cond_var, MCMutex* mutex) {
    pthread_cond_wait(cond_var, mutex);
}

void mcvar_signal(MCCondVar* cond_var) {
    pthread_cond_signal(cond_var);
}

void mcvar_broadcast(MCCondVar* cond_var) {
    pthread_cond_broadcast(cond_var);
}

#endif
