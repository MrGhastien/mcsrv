/**
 * @file
 * @ingroup event
 *
 * General functions regarding the Network sub-system.
 *
 * @addtogroup event
 * @{
 *
 */
#ifndef EVENT_H
#define EVENT_H

#include "definitions.h"
#include "utils/string.h"

/**
 * Data attached to a triggered event.
 */
typedef struct EventInfo {
    /** A pointer to some structure.
     *
     * Should ideally be a structure representing the thread which triggered the event.
     */
    void* sender;
    /**
     * Some data used by event listeners. Can be anything.
     */
    union {
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

/**
 * Event handling function. 
 *
 * Event handlers are called when an event is handled.
 *
 * The @p user_data parameter is used by the thread which registers an event listener
 * to have a pointer to some context structure or whatever is useful when handling an event.
 * The @p info parameter is used by the thread triggering an event to pass data to event handlers.
 *
 * @param event The ID of the triggered event being handled.
 * @param user_data User data passed in when the listener was registered.
 * @param info User data passed in when the event was triggered.
 */
typedef bool (*event_listener)(u32 event, void* user_data, EventInfo info);

/**
 * Initializes the event sub-system.
 *
 * No events can be triggered nor event handlers registered before calling this
 * function. If the sub-system is already initialized, this functions does nothing.
 */
void event_system_init(void);
/**
 * Deinitializes the event sub-system.
 *
 * No events can be triggered nor event handlers registered after calling
 * this function. The @ref event_system_init function must be called to
 * be able to to that. If the sub-system is not initialized, this function
 * does nothing.
 */
void event_system_cleanup(void);

/**
 * Registers a new event type with the given name.
 *
 * If an event with the same ID is already registered, the new event type is not registered.
 *
 * @param code The unique ID of the new event type.
 * @param name The name of the event type to register.
 */
void event_register_event(u32 code, string name);
/**
 * Registers a new event listener for a specific event type.
 *
 * The same event listener can be registered multiple times.
 *
 * @param code The event type ID to register a listener for.
 * @param[in] listener A pointer to the event listening function.
 * @param[in] data Custom user data to pass to the event listener when the event is triggered.
 */
void event_register_listener(u32 code, event_listener listener, void* data);

/**
 * Triggers an event.
 *
 * @param event The ID of the event to trigger.
 * @param info Custom data to pass to event listeners.
 */
void event_trigger(u32 event, EventInfo info);

/**
 * Main routine of the event sub-system.
 *
 * DO NOT call this function (if you do not want another event handler thread).
 */
void event_handle(void);

#endif /* ! EVENT_H */

/** @} */
