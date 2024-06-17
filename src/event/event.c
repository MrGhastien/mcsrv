#include "event.h"
#include "containers/dict.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include <stdio.h>

#define MAX_EVENT_COUNT 256

typedef struct event_listener {
    void* user_data;
    event_handler handler;
} EventListener;

typedef struct event {
    Vector listeners;
} Event;

static Arena arena;
static Dict events;

void event_system_init(void) {
    arena = arena_create(1 << 20);
    dict_init_fixed(&events, &arena, MAX_EVENT_COUNT, sizeof(u32), sizeof(Event));

    log_info("Event subsystem initialized.");
}

void event_system_cleanup(void) {
    arena_destroy(&arena);
}

void event_register_event(u32 code) {
    Event e;
    vector_init_fixed(&e.listeners, &arena, 256, sizeof(EventListener));
    dict_put(&events, &code, &e);
}

void event_register_listener(u32 code, event_handler handler, void* data) {
    u64 idx = dict_get(&events, &code, NULL);
    Event* e = dict_ref(&events, idx);
        
    EventListener listener = {
        .user_data = data,
        .handler   = handler,
    };
    vector_add(&e->listeners, &listener);
}

void event_trigger(u32 event, EventInfo info) {
    (void) event;
    (void) info;

    i64 idx = dict_get(&events, &event, NULL);
    if(idx == -1) {
        fprintf(stderr, "Tried to trigger unregistered event %u.\n", event);
        return;
    }

    //TODO: Parallelize event triggering
    Event* e = dict_ref(&events, idx);
    for(u32 i = 0; i < e->listeners.size; i++) {
        EventListener* listener = vector_ref(&e->listeners, i);
        listener->handler(event, info);
    }
}
