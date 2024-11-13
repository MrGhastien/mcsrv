#include <containers/object_pool.h>
#include <logger.h>
#include <platform/mc_mutex.h>
#ifdef MC_PLATFORM_WINDOWS

#include "platform/mc_thread.h"
#include "definitions.h"

#include <processthreadsapi.h>
#include <windows.h>

#define MAX_THREADS 128

struct ThreadInternal {
    void* arg;
    mcthread_routine routine;
    HANDLE handle;
    i64 index;
    u64 id;
};

// static Dict threads;

static Arena arena;
static ObjectPool threads;
static MCMutex internal_array_mutex;

static bool initialized = FALSE;

void mcthread_init(void) {
    arena = arena_create(MAX_THREADS * (sizeof(struct ThreadInternal) + sizeof(bool)));
    objpool_init(&threads, &arena, MAX_THREADS, sizeof(struct ThreadInternal));
    if(!mcmutex_create(&internal_array_mutex)) {
        log_fatal("Failed to prepare the platform layer for threading.");
        abort();
    }
    initialized = TRUE;
}

static unsigned long routine_wrapper(void* arg) {
    const struct ThreadInternal* wrapper = arg;
    void* res = wrapper->routine(wrapper->arg);
    return (unsigned long) res;
}

i32 mcthread_create(MCThread* thread, mcthread_routine routine, void* arg) {
    if(!routine || !thread)
        return 1;

    // if(!threads.base)
    //     dict_init(&threads, NULL, sizeof(DWORD), sizeof(MCThread*));

    if(!thread->internal)
        return 2;

    i64 index;
    mcmutex_lock(&internal_array_mutex);
    struct ThreadInternal* internal = objpool_add(&threads, &index);
    mcmutex_unlock(&internal_array_mutex);
    internal->arg = arg;
    internal->routine = routine;
    internal->index = index;

    internal->handle = CreateThread(
        NULL,
        0,
        &routine_wrapper,
        internal,
        0,
        &internal->id
        );

    thread->internal = internal;

    // if(dict_put(&threads, &thread->id, &thread) < 0)
    //     return 3;

    return 0;
}
void mcthread_destroy(MCThread* thread) {
    if(!thread)
        return;

    struct ThreadInternal* internal = thread->internal;
    CloseHandle(internal->handle);
    mcmutex_lock(&internal_array_mutex);
    objpool_remove(&threads, internal->index);
    mcmutex_unlock(&internal_array_mutex);

    internal->arg = NULL;
    internal->routine = NULL;
    internal->handle = NULL;
    internal->id = 0;
    thread->internal = NULL;
}

bool mcthread_set_name(const char* name) {
    //TODO
    return FALSE;
}

bool mcthread_create_attachment(MCThreadKey* out_key);
bool mcthread_destroy_attachment(MCThreadKey key);
void mcthread_attach_data(MCThreadKey key, const void* data);
void* mcthread_get_data(MCThreadKey key);

MCThread* mcthread_self(void) {
    //TODO;
}
bool mcthread_equals(MCThread* thread) {
    return FALSE;
}
bool mcthread_is_running(MCThread* thread) {
    return thread->internal != NULL;
}

bool mcthread_join(MCThread* thread, void** out_return) {
    struct ThreadInternal* internal = thread->internal;
    DWORD code = WaitForSingleObject(internal->handle, INFINITE);
    if(code != WAIT_OBJECT_0)
        return FALSE;

    unsigned long res;
    if(!GetExitCodeThread(internal->handle, &res))
        return FALSE;

    mcthread_destroy(thread);

    *out_return = (void*)res;

    return TRUE;
}

#endif