#ifndef LEVEL_H
#define LEVEL_H

#include "containers/dict.h"
#include "memory/allocators/buddy.h"
#include "memory/allocators/pool.h"
#include "utils/position.h"

#include "utils/string.h"

typedef struct chunk Chunk;

typedef struct level {
    Arena arena;
    BuddyAllocator buddy;
    Dict region_dict;
    // Chunks
    // Chunk tickets
    // Chunk sections
    Dict chunk_dict;

    PoolAllocator chunks;
    PoolAllocator regions;
    PoolAllocator chunk_sections;
    PoolAllocator block_entities;
    PoolAllocator entities;

    string path;
} Level;

void level_init(Level* level, string path);

Chunk* level_get_chunk(Level* level, BlockPos pos);

void level_load_chunk(Level* level, ChunkPos pos);
void level_unload_chunk(ChunkPos pos);

#endif /* ! LEVEL_H */
