#ifndef CHUNK_H
#define CHUNK_H

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
} Chunk;

#endif /* ! CHUNK_H */
