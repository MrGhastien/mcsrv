#ifndef MOB_VARIANTS_H
#define MOB_VARIANTS_H

#include "resource/resource_id.h"

enum SpawnConditionType {
    SPAWN_CONDITION_ALWAYS,
    SPAWN_CONDITION_BIOME,
    SPAWN_CONDITION_STRUCTURE,
    SPAWN_CONDITION_MOON_BRIGHTNESS,
};

typedef struct {
    bool is_tag;
    union {
        ResourceID id;
        ResourceID tag;
    } contents;
} IDOrTag;

typedef struct {
    IDOrTag* elements;
    u64 element_count;
} IDOrTagList;

typedef struct {
    i32 priority;
    enum SpawnConditionType type;
    union {
        IDOrTagList biomes_or_structures;
        struct {
            f64 min;
            f64 max;
        } moon_brightness;            
    } data;
} SpawnCondition;

typedef struct {
    ResourceID angry;
    ResourceID wild;
    ResourceID tame;
} WolfVariantAssets;

typedef struct {
    WolfVariantAssets assets;
    /* WolfVariantAssets baby_assets; */
    SpawnCondition* conditions;
    u64 condition_count;
} WolfVariant;

#endif /* ! MOB_VARIANTS_H */
