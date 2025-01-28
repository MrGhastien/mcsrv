#include "logger.h"
#include "level.h"
#include "containers/dict.h"
#include "containers/object_pool.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "utils/bitwise.h"
#include "utils/position.h"
#include <unistd.h>

#define ANVIL_SECTOR_SIZE 4096

typedef struct region {
    int fd;
    RegionPos pos;
} Region;

void level_init(Level* level) {
    level->arena = arena_create(1 << 30, BLK_TAG_LEVEL);

    dict_init_fixed(
        &level->regions, &CMP_VEC2I, &level->arena, 64, sizeof(RegionPos), sizeof(Region));
    dict_init_fixed(&level->chunks, &CMP_VEC2I, &level->arena, 64, sizeof(ChunkPos), sizeof(Chunk));
}

static void read_chunk(Region* region, Chunk* out_chunk, u32 offset, u32 sector_count) {

    lseek(region->fd, offset * ANVIL_SECTOR_SIZE, SEEK_SET);

    u32 length;
    read(region->fd, &length, 4);
    length = untoh32(length);

    u8 compression;
    read(region->fd, &compression, 1);

    if (compression != 4) {
        log_errorf("Compression level %i is not supported.", compression);
        return;
    }


}

static void set_region_offset(Region* region, ChunkPos pos) {
    i64 offset = (pos.x + (pos.y << 5)) << 2;
    lseek(region->fd, offset, SEEK_SET);

    u32 chunk_offset = 0;
    read(region->fd, &chunk_offset, 4);
    u32 sector_count = chunk_offset & 0xff;

    chunk_offset = untoh32(chunk_offset >> 8);

    read_chunk(region, TODO, chunk_offset, sector_count);
}

void level_load_chunk(Level* level, ChunkPos pos) {

    i64 chunk_idx = dict_get(&level->chunks, &pos, NULL);
    if (chunk_idx != -1)
        return;

    RegionPos region_pos = {.x = pos.x >> 5, .y = pos.y >> 5};
    i64 region_idx = dict_get(&level->regions, &region_pos, NULL);

    Region* region;
    if (region_idx == -1) {
    }

    region = dict_ref(&level->regions, region_idx);
    ChunkPos relative_pos = {.x = pos.x & 31, .y = pos.y & 31};

    set_region_offset(region, relative_pos);
}
void level_unload_chunk(ChunkPos pos);
