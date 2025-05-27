#ifndef MEMORY_H
#define MEMORY_H

#include "allocators/arena.h"
#include "allocators/pool.h"

void memory_init(void);
void memory_cleanup(void);

void memory_dump_stats(void);

#endif /* ! MEMORY_H */
