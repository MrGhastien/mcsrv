/**
 * @file hash.h
 * @author Bastien Morino
 * @brief Utility functions related to hashing and comparison.
 */

#ifndef HASH_H
#define HASH_H

#include "definitions.h"

typedef u64 (*hash_function)(const void* data);
typedef i32 (*comparison_function)(const void* lhs, const void* rhs);

typedef struct {
    hash_function hfunc;
    comparison_function comp;
} Comparator;

/**
 * Computes a hash of arbitrary data, accumulating with a previous hash.
 * This can be used to perform hashing of multiple pieces of data without concatenating.
 * @warn The hash computed by this function is not cryptographic.
 *
 * @param[in] hash A hash computed by a previous call to @ref default_hash or @ref default_hash_acc.
 * @param[in] data A buffer containing the data to hash.
 * @param[in] size The size of the data buffer.
 * @returns The hash of the data.
 */
u64 default_hash_acc(u64 hash, const void* data, u64 size);
/**
 * Computes a hash of arbitrary data.
 * @warn The hash computed by this function is not cryptographic.
 *
 * @param[in] data A buffer containing the data to hash.
 * @param[in] size The size of the data buffer.
 * @returns The hash of the data.
 */
u64 default_hash(const void* data, u64 size);
i32 default_cmp(const void* lhs, const void* rhs, u64 size);

u64 cmp_hash(const Comparator* comparator, const void* data, u64 size);
u64 cmp_compare(const Comparator* comparator, const void* lhs, const void* rhs, u64 size);

#endif /* ! HASH_H */
