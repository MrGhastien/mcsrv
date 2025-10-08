#include "level.h"
#include "chunk.h"
#include "containers/dict.h"
#include "data/nbt.h"
#include "logger.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/arena.h"
#include "memory/allocators/buddy.h"
#include "memory/allocators/pool.h"
#include "memory/mem_tags.h"
#include "platform/platform.h"
#include "resource/resource_id.h"
#include "utils/bitwise.h"
#include "utils/iomux.h"
#include "utils/math.h"
#include "utils/position.h"
#include "utils/str_builder.h"
#include "utils/string.h"
#include "world/data/block.h"

#include <assert.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>

#define ANVIL_SECTOR_SIZE 4096

typedef struct region {
    IOMux mux;
    RegionPos pos;
} Region;

void level_init(Level* level, string path) {
    level->arena = arena_create(1 << 30, BLK_TAG_LEVEL, -1);
    level->path  = str_create_copy(&path, &level->arena);

    dict_init(&level->region_dict, &CMP_VEC2I, sizeof(RegionPos), sizeof(i64));
    dict_init(&level->chunk_dict, &CMP_VEC2I, sizeof(ChunkPos), sizeof(i64));

    pool_init_dynamic(&level->regions, 8, sizeof(Region), BLK_TAG_LEVEL, level->arena.chain);
    pool_init_dynamic(&level->chunks, 64, sizeof(Chunk), BLK_TAG_LEVEL, level->regions.mem);
    pool_init_dynamic(
        &level->chunk_sections, 512, sizeof(ChunkSection), BLK_TAG_LEVEL, INVALID_CHAIN);

    buddy_init(&level->buddy, 1 << 24, BLK_TAG_LEVEL);
}

static void read_palette(NBT* nbt, Arena arena, ChunkSection* out_section) {

    nbt_move_to_index(nbt, 0); // 1
    i32 idx = 0;
    do {
        if (nbt_move_to_cstr(nbt, "Name") != NBTE_OK) // 2
            platform_abort();
        string* name = nbt_get_string(nbt);
        log_tracef("    Palette: %s", cstr(name));
        ResourceID id;
        assert(resid_parse(name, &arena, &id));

        StateSelectionContext selector;
        assert(selector_init(&selector, &arena, id));

        nbt_move_to_parent(nbt); // 1
        // If status is not ok, we do not select the 'Properties' tag
        // Thus we don't need to select the parent afterwards
        enum NBTStatus status = nbt_move_to_cstr(nbt, "Properties"); // 2
        if (status == NBTE_OK) {
            nbt_move_to_index(nbt, 0); // 3

            do {
                const string* prop_name = nbt_get_name(nbt);
                log_tracef("      Reading property '%s'", cstr(prop_name));
                const string* prop_value  = nbt_get_string(nbt);
                const StateProperty* prop = get_state_property_by_name(selector.block, *prop_name);
                platform_assert(prop != NULL, "");
                selector_set(&selector, prop, parse_state_property_value(*prop_value, prop));
            } while (nbt_move_to_next_sibling(nbt) == NBTE_OK);

            nbt_move_to_parent(nbt); // 2
            nbt_move_to_parent(nbt); // 1
        } else if (status != NBTE_NOT_FOUND) {
            platform_abort();
        }

        const BlockState* state = selector_select(&selector);
        assert(state != NULL);

        out_section->palette[idx] = state;
        idx++;

    } while (nbt_move_to_next_sibling(nbt) == NBTE_OK);

    nbt_move_to_parent(nbt); // 0
    log_trace("Chunk block palette load OK");
}

static void read_block_indices(NBT* nbt, ChunkSection* section) {
    u64 index_size = u64_log2(section->palette_size) + 1;
    index_size     = max_u64(4, index_size);

    nbt_move_to_index(nbt, 0); // 1

    u64 k = 0;

    do {
        i64 i = nbt_get_long(nbt);
        i64 j = 0;

        while (j < 64) {
            section->indices[k] = (i >> j) & ((1 << index_size) - 1);
            k++;
            j += index_size;
        }

    } while (nbt_move_to_next_sibling(nbt) == NBTE_OK);
    nbt_move_to_parent(nbt); // 0
    log_trace("Chunk block indices load OK");
}

static ChunkSection* read_section(NBT* nbt, Level* level, Arena arena) {
    i32 y;
    nbt_move_to_cstr(nbt, "Y"); // 1
    enum NBTTagType type = nbt_get_type(nbt);
    if (type == NBT_BYTE)
        y = (i32) nbt_get_byte(nbt);
    else if (type == NBT_INT)
        y = nbt_get_int(nbt);
    else {
        log_fatal("Invalid chunk data: Y position of section is neither an INT nor a BYTE tag.");
        abort();
        return NULL;
    }
    nbt_move_to_parent(nbt); // 0

    log_tracef("  Reading section Y=%i", y);

    nbt_move_to_cstr(nbt, "block_states"); // 1
    nbt_move_to_cstr(nbt, "palette");      // 2
    i64 section_idx;
    ChunkSection* section = pool_alloc(&level->chunk_sections, &section_idx);
    section->palette_size = nbt_get_size(nbt);

    section->y_pos   = y;
    section->palette = buddy_alloc(
        &level->buddy, section->palette_size * sizeof(BlockState*) /* , ALLOC_TAG_WORLD */);

    read_palette(nbt, arena, section);
    nbt_move_to_parent(nbt);                        // 1
    if (nbt_move_to_cstr(nbt, "data") == NBTE_OK) { // 2
        section->indices    = buddy_alloc(&level->buddy, 4096 * sizeof(*section->indices));
        section->index_size = 4096;
        read_block_indices(nbt, section);
        nbt_move_to_parent(nbt); // 1
    }

    nbt_move_to_parent(nbt); // 0

    log_trace("Chunk section load OK");

    return section;
}

/**
 * Parses a chunk from a region file.
 */
static void read_chunk(
    Level* level, Region* region, Chunk* out_chunk, u32 offset, u32 sector_count, Arena arena) {
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
        enum NBTStatus status   = nbt_parse(&arena, 8192, compressed_stream, &nbt);
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

    nbt_move_to_cstr(&nbt, "sections"); // 1
    out_chunk->section_count = nbt_get_size(&nbt);
    nbt_move_to_index(&nbt, 0); // 2

    ChunkSection* prev_section = NULL;

    do {
        ChunkSection* section = read_section(&nbt, level, arena);

        if (!prev_section)
            out_chunk->section_head = section;
        else
            prev_section->next = section;
        prev_section = section;
    } while (nbt_move_to_next_sibling(&nbt) == NBTE_OK);
    nbt_move_to_parent(&nbt);
    nbt_move_to_parent(&nbt);
}

/**
 * Locates a chunk inside a region file, and parse the chunk's data.
 */
static void locate_and_read_chunk(Level* level, Region* region, ChunkPos pos) {
    i64 offset = (pos.x + (pos.y << 5)) << 2;
    iomux_seek(region->mux, offset, SEEK_SET);

    u32 chunk_offset = 0;
    iomux_read(region->mux, &chunk_offset, 4);
    chunk_offset     = untoh32(chunk_offset);
    u32 sector_count = chunk_offset & 0xff;
    chunk_offset >>= 8;

    Arena scratch = arena_create(1 << 24, BLK_TAG_LEVEL, level->arena.chain);

    i64 chunk_index;
    Chunk* new_chunk = pool_alloc(&level->chunks, &chunk_index);
    read_chunk(level, region, new_chunk, chunk_offset, sector_count, scratch);
    dict_put(&level->chunk_dict, &pos, &chunk_index);

    arena_destroy(&scratch);
}

/**
 * Loads the chunk at the given (chunk!) position.
 */
void level_load_chunk(Level* level, ChunkPos pos) {

    log_debugf("Loading chunk at position (%lli,%lli)...", pos.x, pos.y);

    i64 chunk_idx;
    if (dict_get(&level->chunk_dict, &pos, &chunk_idx) != -1)
        return;

    RegionPos region_pos = pos_chunk_to_region(pos);
    Region* region;
    i64 region_idx;
    if (dict_get(&level->region_dict, &region_pos, &region_idx) == -1) {

        // If the region file is not opened yet, open it.
        char region_path_buf[PATH_MAX];

        Arena scratch         = level->arena;
        StringBuilder builder = strbuild_create(&scratch);
        strbuild_append(&builder, &level->path);
        strbuild_appends(&builder, "/region/r.");
        strbuild_appendf(&builder, "%lli.%lli.mca", region_pos.x, region_pos.y);
        string region_path = strbuild_to_string_buffer(&builder, region_path_buf, PATH_MAX);

        log_tracef("Opening region file %s...", cstr(&region_path));
        Region* new_region = pool_alloc(&level->regions, &region_idx);
        new_region->mux    = iomux_open(&region_path, "r+b");
        new_region->pos    = region_pos;
        dict_put(&level->region_dict, &region_pos, &region_idx);
    }

    region                = pool_get(&level->regions, region_idx);
    ChunkPos relative_pos = {.x = pos.x & 31, .y = pos.y & 31};

    locate_and_read_chunk(level, region, relative_pos);

    log_debugf("Chunk at position (%lli,%lli) has been successfully loaded!", pos.x, pos.y);
}
void level_unload_chunk(ChunkPos pos);
