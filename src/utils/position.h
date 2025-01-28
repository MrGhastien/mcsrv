#ifndef POSITION_H
#define POSITION_H

#include "definitions.h"
#include "utils/hash.h"

typedef struct vec3i {
    i64 x;
    i64 y;
    i64 z;
} Vec3i;

typedef struct vec3d {
    f64 x;
    f64 y;
    f64 z;
} Vec3d;

typedef struct vec2i {
    i64 x;
    i64 y;
} Vec2i;

typedef Vec3i BlockPos;
typedef Vec3i SectionPos;
typedef Vec2i ChunkPos;
typedef Vec2i RegionPos;

extern const Comparator CMP_VEC3I;
extern const Comparator CMP_VEC3D;
extern const Comparator CMP_VEC2I;

ChunkPos pos_block_to_chunk(BlockPos pos);

/**
 * Computes the hash value of a integer 3D vector.
 *
 * This function should not be used directly. Instead, use the @ref CMP_VEC3I @ref Comparator
 * "comparator".
 *
 * @note This function does not compute a cryptographic hash. It should be used to get a simple
 * hash, e.g. to be used with hash tables.
 *
 * @param[in] str A pointer to integer 3D vector.
 * @return The hash as a unsigned 64-bit integer.
 * @see Comparator
 */
u64 vec3i_hash(const void* str);
/**
 * Compares two integer 3D vectors.
 *
 * @param[in] lhs The left vector operand.
 * @param[in] rhs The right vector operand.
 * @return `0` if the vectors are equal, a negative number if @p lhs is less than @p rhs, and a
 * positive number otherwise.
 */
i32 vec3i_compare(const Vec3i* lhs, const Vec3i* rhs);

/**
 * Computes the hash value of a 3D vector.
 *
 * This function should not be used directly. Instead, use the @ref CMP_VEC3I @ref Comparator
 * "comparator".
 *
 * @note This function does not compute a cryptographic hash. It should be used to get a simple
 * hash, e.g. to be used with hash tables.
 *
 * @param[in] str A pointer to integer 3D vector.
 * @return The hash as a unsigned 64-bit integer.
 * @see Comparator
 */
u64 vec3d_hash(const void* str);
/**
 * Compares two 3D vectors.
 *
 * @param[in] lhs The left vector operand.
 * @param[in] rhs The right vector operand.
 * @return `0` if the vectors are equal, a negative number if @p lhs is less than @p rhs, and a
 * positive number otherwise.
 */
i32 vec3d_compare(const Vec3d* lhs, const Vec3d* rhs);

/**
 * Computes the hash value of a integer 2D vector.
 *
 * This function should not be used directly. Instead, use the @ref CMP_VEC2I @ref Comparator
 * "comparator".
 *
 * @note This function does not compute a cryptographic hash. It should be used to get a simple
 * hash, e.g. to be used with hash tables.
 *
 * @param[in] str A pointer to integer 2D vector.
 * @return The hash as a unsigned 64-bit integer.
 * @see Comparator
 */
u64 vec2i_hash(const void* str);
/**
 * Compares two integer 2D vectors.
 *
 * @param[in] lhs The left vector operand.
 * @param[in] rhs The right vector operand.
 * @return `0` if the vectors are equal, a negative number if @p lhs is less than @p rhs, and a
 * positive number otherwise.
 */
i32 vec2i_compare(const Vec2i* lhs, const Vec2i* rhs);

// TODO: Add utility functions

#endif /* ! POSITION_H */
