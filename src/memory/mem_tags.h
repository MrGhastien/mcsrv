#ifndef MEM_TAGS_H
#define MEM_TAGS_H

#include "definitions.h"
#include "utils/string.h"

/*
  xxxx xxxx xxxx
  ^^^^ ^^^^ ^^^^
  |||| |||| data container
  |||| data format
  server module
 */

enum MemoryTag {
    MEM_TAG_UNKNOWN = 0,
    MEM_TAG_VECTOR = 1,
    MEM_TAG_POOL = 2,
    MEM_TAG_STRING = 3,
    MEM_TAG_DICT = 4,
    MEM_TAG_BYTEBUFFER = 4,

    MEM_TAG_NBT = 1 << 3,
    MEM_TAG_JSON = 2 << 3,
    MEM_TAG_TOML = 2 << 3,

    MEM_TAG_NETWORK = 1 << 6,
    MEM_TAG_EVENT = 2 << 6,
    MEM_TAG_REGISTRY = 3 << 6,
    MEM_TAG_PLATFORM = 4 << 6,

    _MEM_TAG_COUNT = 11,
};

string get_tag_names(i32 tags, Arena* arena);

#endif /* ! MEM_TAGS_H */
