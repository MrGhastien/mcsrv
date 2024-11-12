//
// Created by bmorino on 08/11/2024.
//

#ifndef MC_THREAD_WINDOWS_H
#define MC_THREAD_WINDOWS_H

#include <windows.h>

struct MCThread {
    HANDLE handle;
};

typedef unsigned int MCThreadKey;

#endif /* MC_THREAD_WINDOWS_H */
