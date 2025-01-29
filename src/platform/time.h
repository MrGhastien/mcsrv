//
// Created by bmorino on 09/01/2025.
//

#ifndef TIME_H
#define TIME_H

#include "definitions.h"
#include <time.h>

bool timestamp(struct timespec* out_timestamp);

void milli_sleep(u64 millis);

#endif /* ! TIME_H */
