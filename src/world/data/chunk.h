#ifndef CHUNK_H
#define CHUNK_H

#include "containers/dict.h"
#include "definitions.h"
#include "world/data/block.h"

#define SECTION_SIZE 4096

typedef struct chunk_section {
    u32 palette_size;
    u32 index_size;
    const BlockState* palette;
    //u8* indices;
    u16* indices; // 0 <= index <= 4096
} ChunkSection;

typedef struct chunk {
    u32 section_count;
    ChunkSection* sections;

    Vector block_entities;
    Vector entities;

    Dict entity_mappings;
    Dict block_entity_mappings;
} Chunk;

#endif /* ! CHUNK_H */
