#ifdef MC_PLATFORM_WINDOWS

#include "platform/mc_thread.h"
#include "definitions.h

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

#endif