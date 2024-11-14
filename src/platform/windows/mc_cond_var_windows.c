//
// Created by bmorino on 08/11/2024.
//

#ifdef MC_PLATFORM_WINDOWS
#include "platform/mc_mutex.h"

#include "mc_cond_var_windows.h"

bool mcvar_create(MCCondVar* cond_var) {
    InitializeConditionVariable(cond_var);
    return TRUE;
}
bool mcvar_destroy(MCCondVar* cond_var) {
    // No Win32 function to destroy condition variables ??
    return TRUE;
}

void mcvar_wait(MCCondVar* cond_var, MCMutex* mutex) {
    SleepConditionVariableCS(cond_var, mutex, INFINITE);
}

void mcvar_signal(MCCondVar* cond_var) {
    WakeConditionVariable(cond_var);
}

void mcvar_broadcast(MCCondVar* cond_var) {
    WakeAllConditionVariable(cond_var);
}
#endif
