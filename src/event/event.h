#ifndef EVENT_H
#define EVENT_H

#include "definitions.h"
#include "utils/string.h"

typedef struct event_info {
    void* sender;
    union test_t {
        void* ptrs[2];
        i64 i64[2];
        u64 u64[2];
        f64 f64[2];

        i32 i32[4];
        u32 u32[4];
        f32 f32[4];

        i16 i16[8];
        u16 u16[8];

        i8 i8[16];
        u8 u8[16];
        bool bools[16];
        char chars[16];
    } event_data;
} EventInfo;

enum BuiltinEvents {
    BEVENT_STOP = 0,
    _BEVENT_COUNT
};

typedef bool (*event_handler)(u32 event, void* user_data, EventInfo info);

void event_system_init(void);
void event_system_stop(void);
void event_system_cleanup(void);

void event_register_event(u32 code, string name);
void event_register_listener(u32 code, event_handler listener, void* data);

void event_trigger(u32 event, EventInfo info);

void event_handle(void);

#endif /* ! EVENT_H */
