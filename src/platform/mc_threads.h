#ifndef MC_THREADS_H
#define MC_THREADS_H

#include "definitions.h"
#include "utils/string.h"

typedef struct mc_thread MCThread;

#ifdef MC_PLATFORM_LINUX
#include "mc_threads_linux.h"
#else
#error Not implemented for this platform yet!
#endif

typedef void* (*mcthread_routine)(void* arg);

i32 mcthread_create(MCThread* thread, mcthread_routine routine, void* arg);
void mcthread_destroy(MCThread* thread);

bool mcthread_set_name(const char* name);

bool mcthread_create_attachment(MCThreadKey* out_key);
bool mcthread_destroy_attachment(MCThreadKey key);
void mcthread_attach_data(MCThreadKey key, const void* data);
void* mcthread_get_data(MCThreadKey key);

MCThread* mcthread_self(void);
bool mcthread_equals(MCThread* thread);
bool mcthread_is_running(MCThread* thread);

bool mcthread_join(MCThread* thread, void** out_return);

#endif /* ! MC_THREADS_H */
