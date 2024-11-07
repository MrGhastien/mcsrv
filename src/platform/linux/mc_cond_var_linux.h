#ifndef MC_COND_VAR_LINUX_H
#define MC_COND_VAR_LINUX_H

#include <pthread.h>

struct mc_cond_var {
    pthread_cond_t internal_var;
};

#endif /* ! MC_COND_VAR_LINUX_H */
