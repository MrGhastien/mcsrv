#ifndef EVENT_H
#define EVENT_H

#include "definitions.h"

typedef struct event_info {
    void* user_data;
    void* sender;
} EventInfo;

typedef bool (*event_handler)(u32 code, EventInfo info);

void event_system_init(void);
void event_system_cleanup(void);

void event_register_event(u32 code);
void event_register_listener(u32 code, event_handler listener, void* data);

void event_trigger(u32 event, EventInfo info);

#endif /* ! EVENT_H */
