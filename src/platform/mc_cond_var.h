#ifndef MC_COND_VAR_H
#define MC_COND_VAR_H

#include "definitions.h"
#include "platform/mc_mutex.h"

typedef struct MCCondVar MCCondVar;

bool mcvar_create(MCCondVar* cond_var);
bool mcvar_destroy(MCCondVar* cond_var);

void mcvar_wait(MCCondVar* cond_var, MCMutex* mutex);
void mcvar_signal(MCCondVar* cond_var);
void mcvar_broadcast(MCCondVar* cond_var);

#endif /* ! MC_COND_VAR_H */
