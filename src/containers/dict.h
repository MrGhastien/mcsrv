/**
 * @file
 *
 * A generic dictionnary data structure.
 *
 * This dictionnary interface acts like a hash map:
 * - keys are *unique*,
 * - All keys are of the *same type*,
 * - All values are of the *same type*.
 * - Value lookup is done using a hash function.
 *
 * In practice, only the size of keys (and values) is the same for all keys (or values).
 * No real type checking is done.
 *
 * Some functions (namely @ref dict_put, @ref dict_get and @dict_remove) return indexes to
 * mappings. These indexes should only be used when dealing with fixed dictionnaries, as mapping
 * indexes of dynamic dictionnaries might change when the underlying table is resized.
 *
 * Dictionnaries make use of *hash* and *comparison* functions.
 */
#ifndef DICT_H
#define DICT_H

#include "definitions.h"
#include "memory/arena.h"
#include "utils/hash.h"

/**
 * Internal structure used by dictionnaries to group elements.
 *
 * The implementation does not use this structure to store elements.
 */
struct node {
    u64 hash;    /**< Cached hash. */
    u64* hashp;  /**< Pointer to the hash stored in the dict. */
    void* key;   /**< Pointer to the key stored in the dict.*/
    void* value; /**< Pointer to the value stored in the dict.*/
};

typedef u64 (*hash_function)(const void* key);

/**
 * Structure representing a dictionnary.
 */
typedef struct Dict {
    void* base;                   /**< Memory region containing elements of the dictionnary. */
    const Comparator* comparator; /**< Comparison and hashing functions used to search and add key-value pairs. */
    u64 capacity;                 /**< maximum number of elements that can be stored in the dict. */
    u64 size;                     /**< number of elements in the dict. */
    u64 key_stride;               /**< Size (in bytes) of keys. */
    u64 value_stride;             /**< Size (in bytes) of values. */
    bool fixed;                   /**< True if the underlying array cannot be resized. */
} Dict;

/**
 * Initializes a dynamic dictionnary.
 *
 * Dynamic dictionnaries are automatically resized when their capacity is reached.
 *
 * @param[out] map The dictionnary structure to initialize.
 * @param cmp Comparison and hashing functions used to put elements in and get elements from the dictionnary.
 * @param key_stride The size in bytes of keys.
 * @param value_stride The size in bytes of values.
 */
void dict_init(Dict* map, const Comparator* cmp, u64 key_stride, u64 value_stride);
/**
 * Initializes a fixed dictionnary.
 *
 * Fixed dictionnaries can not be resized.
 *
 * @param[out] map The dictionnary structure to initialize.
 * @param cmp Comparison and hashing functions used to put elements in and get elements from the dictionnary.
 * @param arena The arena to use to allocate underlying memory.
 * @param key_stride The size in bytes of keys.
 * @param value_stride The size in bytes of values.
 */
void dict_init_fixed(
    Dict* map, const Comparator* cmp, Arena* arena, u64 capacity, u64 key_stride, u64 value_stride);

/**
 * Frees memory associated with a dictionnary.
 *
 * If the given dictionnary is fixed, this function does nothing.
 * @param map The dictionnary to destory.
 */
void dict_destroy(Dict* map);

/**
 * Inserts a mapping between a key and a value inside a dictionnary.
 *
 * The key and value are copied inside the dictionnary's internal buffer.
 * @param map The dictionnary to remove the mapping from.
 * @param key A pointer to the key to create a mapping for.
 * @param value A pointer to the value to create a mapping for.
 * @return The index of the newly created mapping inside of the dictionnary.
 */
i64 dict_put(Dict* map, const void* key, const void* value);
/**
 * Removes a mapping between a key and a value inside a dictionnary.
 *
 * This function removes the mapping with the given key, and returns the value of such mapping (if it existed)
 * @param map The dictionnary to remove the mapping from.
 * @param key A pointer to the key of the mapping to remove.
 * @param[out] out_value A pointer to a memory region that will contain the value of the deleted mapping.
 * @return The index of the mapping inside of the dictionnary, or `-1` if no mapping with the
 * given key exists.
 */
i64 dict_remove(Dict* map, const void* key, void* outValue);

/**
 * Retrieves the mapping having the specified key from a dictionnary.
 *
 * @param map The dictionnary to query.
 * @param key A pointer to the key to find a mapping with.
 * @param out_value A pointer to a memory region that will contain the value of the retrieved
 * mapping.
 * @return The index of the mappin inside of the dictionnary, or `-1` if no mapping with the
 * given key exists.
 */
i64 dict_get(Dict* map, const void* key, void* outValue);
/**
 * Retrieves a reference to a mapping's value.
 *
 * @param[in] dict The dictionnary to query.
 * @param idx The index of the mapping reference to retrieve.
 * @return A pointer to the mapping's value at the given index, or `NULL` if the index is
 * out of bounds or no mapping is present at that index.
 */
void* dict_ref(Dict* dict, i64 idx);

/**
 * An action on a dictionnary's mapping.
 *
 * @param dict The dictionnary in which the mapping is stored.
 * @param idx The index of the mapping inside of the dictionnary.
 * @param [in] key The mapping's key.
 * @param [in] value The mapping's value.
 * @param data Arbitrary data / context to use when processing the mapping.
 */
typedef void (*action)(const Dict* dict, u64 idx, void* key, void* value, void* data);
/**
 * Execute the given action for each mapping of a dictionnary.
 *
 * Callers can pass context data to actions through the @p data parameter.
 * @param map The dictionnary to iterate.
 * @param action[in] The action to execute.
 * @param data Arbitrary data to use in each iteration.
 */
void dict_foreach(const Dict* map, action action, void* data);

#endif /* ! DICT_H */
