//
// Created by bmorino on 08/11/2024.
//

#ifndef MC_MUTEX_WINDOWS_H
#define MC_MUTEX_WINDOWS_H

#include <windows.h>

struct MCMutex {
    HANDLE internal_lock;
};

#endif /* MC_MUTEX_WINDOWS_H */
