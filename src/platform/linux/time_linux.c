//
// Created by bmorino on 09/01/2025.
//

#ifdef MC_PLATFORM_LINUX

#include "platform/time.h"

bool timestamp(struct timespec *out_timestamp) {
    return clock_gettime(CLOCK_MONOTONIC, out_timestamp) == 0;
}

void milli_sleep(u64 millis) {
    usleep(millis * 1000);
}

#endif