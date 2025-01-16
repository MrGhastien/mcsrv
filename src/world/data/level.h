#ifndef LEVEL_H
#define LEVEL_H

#include "containers/dict.h"
#include "utils/position.h"
#include "chunk.h"

typedef struct level {
    // Chunks
    // Chunk tickets
    // Chunk sections
    Dict chunks;
} Level;

Chunk* level_get_chunk(BlockPos pos);

#endif /* ! LEVEL_H */
