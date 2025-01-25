#ifndef MEM_TAGS_H
#define MEM_TAGS_H

typedef struct str string;

enum AllocTag {
    ALLOC_TAG_UNKNOWN = 0,
    ALLOC_TAG_VECTOR,
    ALLOC_TAG_POOL,
    ALLOC_TAG_STRING,
    ALLOC_TAG_DICT,
    ALLOC_TAG_BYTEBUFFER,
    ALLOC_TAG_PACKET,
    ALLOC_TAG_JSON,
    ALLOC_TAG_NBT,
    ALLOC_TAG_WORLD,

    ALLOC_TAG_EXTERNAL,

    _ALLOC_TAG_COUNT,
};

enum MemoryBlockTag {
    BLK_TAG_UNKNOWN = 0,
    BLK_TAG_NETWORK,
    BLK_TAG_EVENT,
    BLK_TAG_REGISTRY,
    BLK_TAG_PLATFORM,
    BLK_TAG_MEMORY,
    BLK_TAG_DATA,

    _BLK_TAG_COUNT,
};
string get_alloc_tag_name(enum AllocTag tag);
string get_blk_tag_name(enum MemoryBlockTag tag);

//void set_global_tags(i32 tags);
void memory_stats_init(void);
void memory_dump_stats(void);

#endif /* ! MEM_TAGS_H */
