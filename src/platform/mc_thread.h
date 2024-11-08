#ifndef MC_THREADS_H
#define MC_THREADS_H

#include "definitions.h"
#include "utils/string.h"


#ifdef MC_PLATFORM_LINUX
#include "linux/mc_thread_linux.h"
#elif defined MC_PLATFORM_WINDOWS
#include "windows/mc_thread_windows.h"
#else
#error Not implemented for this platform yet!
#endif

typedef struct MCThread MCThread;

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
