//
// Created by bmorino on 29/01/2025.
//

#include "simulation.h"
#include "world/data/level.h"

#include "platform/mc_thread.h"
#include "platform/time.h"

#include <stdlib.h>

static MCThread thread;
static bool running = FALSE;

static void* simulate(void*) {
    Level level;
    level_init(&level);

    level_load_chunk(&level, CHUNK_POS(0, 0));

    while (running) {
        milli_sleep(500);
    }
    return NULL;
}

void sim_start(void) {
    running = TRUE;
    mcthread_create(&thread, &simulate, NULL);
}

void sim_stop(void) {
    running = FALSE;
    mcthread_join(&thread, NULL);
}
