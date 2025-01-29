//
// Created by bmorino on 09/01/2025.
//

#include <network/common_types.h>
#ifdef MC_PLATFORM_WINDOWS

#include "platform/time.h"

#define NS_PER_SECOND 1000000000l

bool timestamp(struct timespec *out_timestamp) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER ticks;

    if(frequency.QuadPart == 0 && !QueryPerformanceFrequency(&frequency)) {
        return FALSE;
    }

    if(!QueryPerformanceCounter(&ticks)) {
        return FALSE;
    }

    out_timestamp->tv_sec = (time_t)ticks.QuadPart / frequency.QuadPart;
    out_timestamp->tv_nsec = (long)((ticks.QuadPart % frequency.QuadPart) * NS_PER_SECOND) / frequency.QuadPart;

    return TRUE;
}

void milli_sleep(u64 millis) {
    Sleep(millis);
}

#endif