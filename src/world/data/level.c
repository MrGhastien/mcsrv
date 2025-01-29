#include "level.h"
#include "chunk.h"
#include "containers/dict.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "utils/bitwise.h"
#include "utils/iomux.h"
#include "utils/position.h"
#include "data/nbt.h"

#include <stdio.h>

#define ANVIL_SECTOR_SIZE 4096

typedef struct region {
    IOMux mux;
    RegionPos pos;
} Region;

void level_init(Level* level) {
    level->arena = arena_create(1 << 30, BLK_TAG_LEVEL);

    dict_init_fixed(
        &level->regions, &CMP_VEC2I, &level->arena, 64, sizeof(RegionPos), sizeof(Region));
    dict_init_fixed(&level->chunks, &CMP_VEC2I, &level->arena, 64, sizeof(ChunkPos), sizeof(Chunk));
}

static void
read_chunk(Region* region, Chunk* out_chunk, u32 offset, u32 sector_count, Arena arena) {
    UNUSED(sector_count);

    iomux_seek(region->mux, offset * ANVIL_SECTOR_SIZE, SEEK_SET);

    u32 length;
    iomux_read(region->mux, &length, 4);
    length = untoh32(length);

    u8 compression;
    iomux_read(region->mux, &compression, 1);

    if (compression != 4) {
        log_errorf("Compression level %i is not supported.", compression);
        return;
    }

    NBT nbt;
    IOMux compressed_stream = iomux_wrap_zlib(region->mux, &arena);
    nbt_parse(&arena, 8192, compressed_stream, &nbt);
    iomux_close(compressed_stream);

    nbt_move_to_cstr(&nbt, "sections");
    nbt_move_to_index(&nbt, 0);

    do {
        i32 y;
        enum NBTTagType type = nbt_get_type(&nbt);
        if (type == NBT_BYTE)
            y = (i32) nbt_get_byte(&nbt);
        else if (type == NBT_INT)
            y = nbt_get_int(&nbt);
        else {
            abort();
        }
        nbt_move_to_cstr(&nbt, "block_states");
        nbt_move_to_cstr(&nbt, "palette");
        ChunkSection* section = &out_chunk->sections[y];
        section->palette_size = nbt_get_size(&nbt);
    } while (nbt_move_to_next_sibling(&nbt) == NBTE_OK);
}

static void locate_and_read_chunk(Level* level, Region* region, ChunkPos pos) {
    i64 offset = (pos.x + (pos.y << 5)) << 2;
    iomux_seek(region->mux, offset, SEEK_SET);

    u32 chunk_offset = 0;
    iomux_read(region->mux, &chunk_offset, 4);
    u32 sector_count = chunk_offset & 0xff;

    chunk_offset = untoh32(chunk_offset >> 8);

    Chunk chunk;
    read_chunk(region, &chunk, chunk_offset, sector_count, level->arena);
    dict_put(&level->chunks, &pos, &chunk);
}

void level_load_chunk(Level* level, ChunkPos pos) {

    i64 chunk_idx = dict_get(&level->chunks, &pos, NULL);
    if (chunk_idx != -1)
        return;

    RegionPos region_pos = {.x = pos.x >> 5, .y = pos.y >> 5};
    i64 region_idx = dict_get(&level->regions, &region_pos, NULL);

    Region* region;
    if (region_idx == -1) {
        // TODO: Add region and open file
    }

    region = dict_ref(&level->regions, region_idx);
    ChunkPos relative_pos = {.x = pos.x & 31, .y = pos.y & 31};

    locate_and_read_chunk(level, region, relative_pos);
}
void level_unload_chunk(ChunkPos pos);
