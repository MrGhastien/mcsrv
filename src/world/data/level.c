#include "level.h"
#include "chunk.h"
#include "containers/dict.h"
#include "memory/allocators/pool.h"
#include "data/json.h"
#include "data/nbt.h"
#include "logger.h"
#include "memory/allocators/arena.h"
#include "memory/mem_tags.h"
#include "resource/resource_id.h"
#include "utils/bitwise.h"
#include "utils/iomux.h"
#include "utils/position.h"
#include "utils/str_builder.h"
#include "utils/string.h"
#include "world/data/block.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define ANVIL_SECTOR_SIZE 4096

typedef struct region {
    IOMux mux;
    RegionPos pos;
} Region;

void level_init(Level* level, string path) {
    level->arena = arena_create(1 << 30, BLK_TAG_LEVEL, -1);
    level->path = str_create_copy(&path, &level->arena);

    dict_init(
        &level->region_dict, &CMP_VEC2I, sizeof(RegionPos), sizeof(i64));
    dict_init(
        &level->chunk_dict, &CMP_VEC2I, sizeof(ChunkPos), sizeof(i64));

    pool_init_dynamic(&level->regions, 8, sizeof(Region), BLK_TAG_LEVEL, level->arena.chain);
    pool_init_dynamic(&level->chunks, 64, sizeof(Chunk), BLK_TAG_LEVEL, level->regions.mem);
    pool_init_dynamic(&level->chunk_sections, 512, sizeof(ChunkSection), BLK_TAG_LEVEL, level->chunk_sections.mem);
}

static void
read_chunk(Region* region, Chunk* out_chunk, u32 offset, u32 sector_count, Arena arena) {
    UNUSED(sector_count);

    iomux_seek(region->mux, offset * ANVIL_SECTOR_SIZE, SEEK_SET);

    u32 length;
    iomux_read(region->mux, &length, 4);
    length = untoh32(length) - 1; // quirk of the anvil file format

    u8 compression;
    iomux_read(region->mux, &compression, 1);

    assert(compression == 2 || compression == 0);

    NBT nbt;
    switch (compression) {
    case 2: {
        IOMux compressed_stream = iomux_wrap_zlib(region->mux, length, &arena);
        enum NBTStatus status = nbt_parse(&arena, 8192, compressed_stream, &nbt);
        iomux_close(compressed_stream);
        if (status != NBTE_OK) {
            log_fatalf("An error occurred when reading a chunk's data: %i", status);
            abort();
        }
        break;
    }
    case 0:
        nbt_parse(&arena, 8192, region->mux, &nbt);
        break;
    default:
        log_errorf("Compression level %i is not supported.", compression);
        return;
    }

    nbt_move_to_cstr(&nbt, "sections");
    out_chunk->section_count = nbt_get_size(&nbt);
    nbt_move_to_index(&nbt, 0);

    do {
        i32 y;
        nbt_move_to_cstr(&nbt, "Y");
        enum NBTTagType type = nbt_get_type(&nbt);
        if (type == NBT_BYTE)
            y = (i32) nbt_get_byte(&nbt);
        else if (type == NBT_INT)
            y = nbt_get_int(&nbt);
        else {
            log_fatal(
                "Invalid chunk data: Y position of section is neither an INT nor a BYTE tag.");
            abort();
            return;
        }
        nbt_move_to_parent(&nbt);
        nbt_move_to_cstr(&nbt, "block_states");
        nbt_move_to_cstr(&nbt, "palette");
        ChunkSection* section = &out_chunk->sections[y];
        section->palette_size = nbt_get_size(&nbt);

        section->palette =
            arena_callocate(&arena, section->palette_size * sizeof(BlockState*)/* , ALLOC_TAG_WORLD */);
        nbt_move_to_index(&nbt, 0);
        i32 idx = 0;

        do {
            nbt_move_to_cstr(&nbt, "Name");
            string* name = nbt_get_string(&nbt);
            ResourceID id;
            assert(resid_parse(name, &arena, &id));

            StateSelectionContext selector;
            assert(selector_init(&selector, &arena, id));

            nbt_move_to_parent(&nbt);
            nbt_move_to_cstr(&nbt, "Properties");
            nbt_move_to_index(&nbt, 0);

            do {
                const string* prop_name = nbt_get_name(&nbt);
                const string* prop_value = nbt_get_string(&nbt);
                const StateProperty* prop = get_state_property_by_name(*prop_name);
                assert(prop != NULL);
                selector_set(&selector, prop, parse_state_property_value(*prop_value, prop));
            } while(nbt_move_to_next_sibling(&nbt) == NBTE_OK);
            
            const BlockState* state = selector_select(&selector);
            assert(state != NULL);

            section->palette[idx] = state;
            idx++;

        } while (nbt_move_to_next_sibling(&nbt) == NBTE_OK);

    } while (nbt_move_to_next_sibling(&nbt) == NBTE_OK);
}

static void locate_and_read_chunk(Level* level, Region* region, ChunkPos pos) {
    i64 offset = (pos.x + (pos.y << 5)) << 2;
    iomux_seek(region->mux, offset, SEEK_SET);

    u32 chunk_offset = 0;
    iomux_read(region->mux, &chunk_offset, 4);
    chunk_offset = untoh32(chunk_offset);
    u32 sector_count = chunk_offset & 0xff;
    chunk_offset >>= 8;

    log_fatal("TODO: Actually allocate the section array !");
    abort();
    i64 chunk_index;
    Chunk* new_chunk = pool_alloc(&level->chunks, &chunk_index);
    read_chunk(region, new_chunk, chunk_offset, sector_count, level->arena);
    dict_put(&level->chunk_dict, &pos, &chunk_index);
}

void level_load_chunk(Level* level, ChunkPos pos) {

    log_tracef("Loading chunk at position (%lli,%lli)...", pos.x, pos.y);

    i64 chunk_idx;
    if(dict_get(&level->chunk_dict, &pos, &chunk_idx) != -1)
        return;

    RegionPos region_pos = pos_chunk_to_region(pos);
    Region* region;
    i64 region_idx;
    if(dict_get(&level->region_dict, &region_pos, &region_idx) == -1) {
        Arena scratch = level->arena;
        StringBuilder builder = strbuild_create(&scratch);
        strbuild_append(&builder, &level->path);
        strbuild_appends(&builder, "/region/r.");
        strbuild_appendf(&builder, "%lli.%lli.mca", region_pos.x, region_pos.y);
        string region_path = strbuild_to_string(&builder, &level->arena);

        log_tracef("Opening region file %s...", cstr(&region_path));
        Region* new_region = pool_alloc(&level->regions, &region_idx);
        new_region->mux = iomux_open(&region_path, "r+b");
        new_region->pos = region_pos;
        dict_put(&level->region_dict, &region_pos, &region_idx);
    }

    region = pool_get(&level->regions, region_idx);
    ChunkPos relative_pos = {.x = pos.x & 31, .y = pos.y & 31};

    locate_and_read_chunk(level, region, relative_pos);
}
void level_unload_chunk(ChunkPos pos);
