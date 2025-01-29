#include "event.h"
#include "containers/dict.h"
#include "containers/ring_queue.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"

#include "platform/mc_thread.h"
#include "platform/mc_mutex.h"
#include "platform/mc_cond_var.h"

#include <stdio.h>

#define MAX_EVENT_COUNT 256
#define MAX_TRIGGERED_EVENTS 32

typedef struct EventListener {
    void* user_data;
    event_listener handler;
} EventListener;

typedef struct EventEntry {
    Vector listeners;
    string name;
} EventEntry;

typedef struct TriggeredEvent {
    EventInfo info;
    u32 code;
} TriggeredEvent;

typedef struct EventContext {
    Arena arena;
    Dict event_registry;
    MCMutex mutex;
    MCCondVar cond_var;
    RingQueue queue;
    bool running;
} EventContext;

static EventContext ctx;

static void register_builtin_events(void) {
    event_register_event(BEVENT_STOP, str_view("STOP"));
}

void event_system_init(void) {
    ctx.arena = arena_create(1L << 20, BLK_TAG_EVENT);

    dict_init_fixed(
                    &ctx.event_registry, NULL, &ctx.arena, MAX_EVENT_COUNT, sizeof(u32), sizeof(EventEntry));
    ctx.queue = rqueue_create(MAX_TRIGGERED_EVENTS, sizeof(TriggeredEvent), &ctx.arena);

    mcmutex_create(&ctx.mutex);
    mcvar_create(&ctx.cond_var);

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
        EventListener* listener = vect_ref(&entry->listeners, i);
        listener->handler(BEVENT_STOP, listener->user_data, info);
    }
}

void event_system_cleanup(void) {
    broadcast_stop_event();

    arena_destroy(&ctx.arena);

    mcmutex_destroy(&ctx.mutex);
    mcvar_destroy(&ctx.cond_var);
}

void event_register_event(u32 code, string name) {
    mcmutex_lock(&ctx.mutex);

    i64 idx = dict_get(&ctx.event_registry, &code, NULL);
    if (idx >= 0) {
        log_errorf("Can not register the already registered event %u.", code);
        mcmutex_unlock(&ctx.mutex);
        return;
    }
    EventEntry e;
    e.name = name;
    vect_init(&e.listeners, &ctx.arena, 256, sizeof(EventListener));
    dict_put(&ctx.event_registry, &code, &e);

    mcmutex_unlock(&ctx.mutex);
}

void event_register_listener(u32 code, event_listener handler, void* data) {
    if(code == BEVENT_STOP) {
        log_error("The STOP event cannot be listened to.");
        return;
    }

    if (!handler) {
        log_error("Attempt to register an empty event listener.");
        return;
    }

    mcmutex_lock(&ctx.mutex);

    i64 idx = dict_get(&ctx.event_registry, &code, NULL);
    if (idx == -1) {
        log_errorf("Cannot register listener for unregistered event %u.", code);
        mcmutex_unlock(&ctx.mutex);
        return;
    }
    EventEntry* e = dict_ref(&ctx.event_registry, idx);

    EventListener listener = {
        .user_data = data,
        .handler   = handler,
    };
    vect_add(&e->listeners, &listener);

    mcmutex_unlock(&ctx.mutex);
}

void event_trigger(u32 event, EventInfo info) {
    TriggeredEvent e = {
        .code = event,
        .info = info,
    };
    mcmutex_lock(&ctx.mutex);

    rqueue_enqueue(&ctx.queue, &e);

    mcmutex_unlock(&ctx.mutex);

    log_tracef("New event received (%u).", event);
    mcvar_signal(&ctx.cond_var);
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
            EventListener* listener = vect_ref(&entry->listeners, i);
            listener->handler(e->code, listener->user_data, e->info);
            //log_tracef("Notified %p of event %u.", listener->user_data, e->code);
        }
        rqueue_dequeue(&ctx.queue, NULL);
    }
    return !stop_event;
}

void event_handle(void) {
    while (ctx.running) {
        mcmutex_lock(&ctx.mutex);
        if (ctx.queue.length == 0)
            mcvar_wait(&ctx.cond_var, &ctx.mutex);

        ctx.running = process_event_queue();

        mcmutex_unlock(&ctx.mutex);
    }
}
