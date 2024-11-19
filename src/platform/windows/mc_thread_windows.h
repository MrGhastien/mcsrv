//
// Created by bmorino on 13/11/2024.
//

#ifndef MCTHREAD_WINDOWS_H
#define MCTHREAD_WINDOWS_H

typedef struct {
    struct ThreadInternal* internal;
} MCThread;

typedef i32 MCThreadKey;

#endif /* ! MCTHREAD_WINDOWS_H */
