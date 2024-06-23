#include "event.h"
#include "containers/dict.h"
#include "containers/ring_queue.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"

#include <pthread.h>
#include <stdio.h>

#define MAX_EVENT_COUNT 256
#define MAX_TRIGGERED_EVENTS 32

typedef struct event_listener {
    void* user_data;
    event_handler handler;
} EventListener;

typedef struct event {
    Vector listeners;
    string name;
} EventEntry;

typedef struct triggered_event {
    EventInfo info;
    u32 code;
} TriggeredEvent;

typedef struct event_ctx {
    Arena arena;
    Dict event_registry;
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    RingQueue queue;
    bool running;
} EventContext;

static EventContext ctx;

static void register_builtin_events(void) {
    event_register_event(BEVENT_STOP, str_create_const("STOP"));
}

void event_system_init(void) {
    ctx.arena = arena_create(1L << 20);

    dict_init_fixed(
        &ctx.event_registry, &ctx.arena, MAX_EVENT_COUNT, sizeof(u32), sizeof(EventEntry));
    ctx.queue = rqueue_create(MAX_TRIGGERED_EVENTS, sizeof(TriggeredEvent), &ctx.arena);

    pthread_mutex_init(&ctx.mutex, 0);
    pthread_cond_init(&ctx.cond_var, 0);

    register_builtin_events();

    ctx.running = TRUE;
    log_debug("Event subsystem initialized.");
}

static void broadcast_stop_event(void) {
    u32 code          = BEVENT_STOP;
    i64 idx           = dict_get(&ctx.event_registry, &code, NULL);
    EventEntry* entry = dict_ref(&ctx.event_registry, idx);

    EventInfo info = {
        .event_data = {},
        .sender     = NULL,
    };
    for (u32 i = 0; i < entry->listeners.size; i++) {
        EventListener* listener = vector_ref(&entry->listeners, i);
        listener->handler(BEVENT_STOP, listener->user_data, info);
    }
}

void event_system_cleanup(void) {
    broadcast_stop_event();

    arena_destroy(&ctx.arena);

    pthread_mutex_destroy(&ctx.mutex);
    pthread_cond_destroy(&ctx.cond_var);
}

void event_register_event(u32 code, string name) {
    pthread_mutex_lock(&ctx.mutex);

    i64 idx = dict_get(&ctx.event_registry, &code, NULL);
    if (idx >= 0) {
        log_errorf("Can not register the already registered event %u.", code);
        pthread_mutex_unlock(&ctx.mutex);
        return;
    }
    EventEntry e;
    e.name = name;
    vector_init_fixed(&e.listeners, &ctx.arena, 256, sizeof(EventListener));
    dict_put(&ctx.event_registry, &code, &e);

    pthread_mutex_unlock(&ctx.mutex);
}

void event_register_listener(u32 code, event_handler handler, void* data) {
    if(code == BEVENT_STOP) {
        log_error("The STOP event cannot be listened to.");
        return;
    }

    if (!handler) {
        log_error("Attempt to register an empty event listener.");
        return;
    }

    pthread_mutex_lock(&ctx.mutex);

    i64 idx = dict_get(&ctx.event_registry, &code, NULL);
    if (idx == -1) {
        log_errorf("Cannot register listener for unregistered event %u.", code);
        pthread_mutex_unlock(&ctx.mutex);
        return;
    }
    EventEntry* e = dict_ref(&ctx.event_registry, idx);

    EventListener listener = {
        .user_data = data,
        .handler   = handler,
    };
    vector_add(&e->listeners, &listener);

    pthread_mutex_unlock(&ctx.mutex);
}

void event_trigger(u32 event, EventInfo info) {
    TriggeredEvent e = {
        .code = event,
        .info = info,
    };
    pthread_mutex_lock(&ctx.mutex);

    rqueue_enqueue(&ctx.queue, &e);

    pthread_mutex_unlock(&ctx.mutex);

    log_tracef("New event received (%u).", event);
    pthread_cond_signal(&ctx.cond_var);
}

static bool process_event_queue(void) {
    bool stop_event = FALSE;
    while (ctx.queue.length > 0) {
        const TriggeredEvent* e = rqueue_peek(&ctx.queue);

        i64 idx = dict_get(&ctx.event_registry, &e->code, NULL);
        if (idx == -1) {
            log_errorf("An unregistered event (%u) was triggered!.", e->code);
            continue;
        }

        if(e->code == BEVENT_STOP)
            stop_event = TRUE;

        EventEntry* entry = dict_ref(&ctx.event_registry, idx);

        // Notify each listener
        for (u32 i = 0; i < entry->listeners.size; i++) {
            EventListener* listener = vector_ref(&entry->listeners, i);
            listener->handler(e->code, listener->user_data, e->info);
            //log_tracef("Notified %p of event %u.", listener->user_data, e->code);
        }
        rqueue_dequeue(&ctx.queue, NULL);
    }
    return !stop_event;
}

void event_handle(void) {

    while (ctx.running) {
        pthread_mutex_lock(&ctx.mutex);
        if (ctx.queue.length == 0)
            pthread_cond_wait(&ctx.cond_var, &ctx.mutex);

        ctx.running = process_event_queue();

        pthread_mutex_unlock(&ctx.mutex);
    }
}
