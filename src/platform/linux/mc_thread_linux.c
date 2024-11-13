#ifdef MC_PLATFORM_LINUX

#include "platform/mc_thread.h"
#include "logger.h"

#include <pthread.h>
#include <string.h>
#include <sys/prctl.h>
#include <errno.h>

static MCThreadKey self_key;
static pthread_once_t once_control;

static void init_routine(void) {
    mcthread_create_attachment(&self_key);
}

i32 mcthread_create(MCThread* thread, mcthread_routine routine, void* arg) {

    pthread_once(&once_control, &init_routine);

    i32 res = pthread_create(thread, NULL, routine, arg);
    if (res) {
        log_fatalf("Failed to create thread: %s.", strerror(res));
        return res;
    }
    mcthread_attach_data(self_key, thread);
    return 0;
}

bool mcthread_set_name(const char* name) {
    return prctl(PR_SET_NAME, name, 0) == 0;
}

void mcthread_destroy(MCThread* thread) {
    pthread_cancel(*thread);
}

bool mcthread_create_attachment(MCThreadKey* out_key) {
    i32 res = pthread_key_create(out_key, NULL);
    if (res) {
        log_fatalf("Failed to create thread attachment: %s.", strerror(res));
        return FALSE;
    }
    return TRUE;
}

bool mcthread_delete_attachment(MCThreadKey key) {
    i32 res = pthread_key_delete(key);
    if (res) {
        log_fatalf("Failed to destroy thread attachment: %s.", strerror(res));
        return FALSE;
    }
    return TRUE;
}

void mcthread_attach_data(MCThreadKey key, const void* data) {
    if (pthread_setspecific(key, data))
        log_error("The given attachment key is invalid.");
}

void* mcthread_get_data(MCThreadKey key) {
    return pthread_getspecific(key);
}

//TODO: Fix this!
MCThread* mcthread_self(void) {
    return mcthread_get_data(self_key);
}

bool mcthread_equals(MCThread* thread) {
    pthread_t self = pthread_self();
    return pthread_equal(self, *thread);
}

bool mcthread_join(MCThread* thread, void** out_return) {
    i32 res = pthread_join(*thread, out_return);
    switch (res) {
    case EDEADLK:
        log_error("Unable to join thread: A deadlock has been detected.");
        return FALSE;
    case ESRCH:
        log_error("Could not join an unknown thread.");
        return FALSE;
    case EINVAL:
        log_error("Unable to join thread: the thread is already joining or is not joinable.");
        return FALSE;
    default:
        return TRUE;
    }
}

#endif
