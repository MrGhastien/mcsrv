#include "position.h"

i32 vec2i_compare_raw(const void* lhs, const void* rhs) {
    return vec2i_compare(lhs, rhs);
}

const Comparator CMP_VEC3I;
const Comparator CMP_VEC3D;
const Comparator CMP_VEC2I = {
    .comp = &vec2i_compare_raw,
    .hfunc = &vec2i_hash,
};

ChunkPos pos_block_to_chunk(BlockPos pos) {
    return (ChunkPos) {
        .x = pos.x >> 4,
        .y = pos.y >> 4,
    };
}

u64 vec3i_hash(const void* str);
i32 vec3i_compare(const Vec3i* lhs, const Vec3i* rhs);

u64 vec3d_hash(const void* str);
i32 vec3d_compare(const Vec3d* lhs, const Vec3d* rhs);

u64 vec2i_hash(const void* ptr) {
    const Vec2i* vec = ptr;
    u64 res = 1;
    res = 31 * res + vec->x;
    res = 31 * res + vec->y;
    return res;
}
i32 vec2i_compare(const Vec2i* lhs, const Vec2i* rhs) {
    i64 diff = 0;
    if (lhs->x != rhs->x)
        diff = lhs->x - rhs->x;
    else if (lhs->y != rhs->y)
        diff = lhs->y - rhs->y;
    return (diff >> 32) & 0xffffffff;
}
