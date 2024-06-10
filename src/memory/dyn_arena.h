#ifndef LINALLOC_H
#define LINALLOC_H

#include "../definitions.h"
#include "arena.h"
#include <stddef.h>

/**
   A more complex allocatr than an @{Arena}.

   The allocated memory is not guaranteed to be continuous,
   but there is no limit to how many memory can be allocated.

   This is basically a vector of Arenas.
 */
typedef struct linalloc {
    Arena* blocks;
    size_t block_count;
    size_t block_capacity;
} DynamicArena;

DynamicArena dynarena_create();
void dynarena_reserve(DynamicArena* arena, size_t bytes);
void dynarena_destroy(DynamicArena* arena);

void* dynarena_allocate(DynamicArena* arena, size_t bytes);
void* dynarena_callocate(DynamicArena* arena, size_t bytes);
void dynarena_free(DynamicArena* arena, size_t bytes);
void dynarena_free_ptr(DynamicArena* arena, void* ptr);

#endif /* ! LINALLOC_H */
