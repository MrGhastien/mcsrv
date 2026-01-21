//
// Created by bmorino on 29/01/2025.
//

#include "simulation.h"
#include "world/data/level.h"

#include "platform/mc_thread.h"
#include "platform/time.h"

#include <stdlib.h>

static MCThread thread;
static bool running = false;

static void* simulate(void* unused) {
    UNUSED(unused);
    Level level;
    level_init(&level, str_view("./world"));

    level_load_chunk(&level, CHUNK_POS(-1, 2));

    while (running) {
        milli_sleep(500);
    }

    level_destroy(&level);
    return NULL;
}

void sim_start(void) {
    running = true;
    mcthread_create(&thread, &simulate, NULL);
}

void sim_stop(void) {
    running = false;
    mcthread_join(&thread, NULL);
}
