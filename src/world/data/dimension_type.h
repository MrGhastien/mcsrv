#ifndef DIMENSION_TYPE_H
#define DIMENSION_TYPE_H

#include "resource/resource_id.h"

enum IntProviderType {
    INT_PROVIDER_CONSTANT = 0,
    INT_PROVIDER_UNIFORM,
    INT_PROVIDER_BIASED_TO_BOTTOM,
    INT_PROVIDER_CLAMPED,
    INT_PROVIDER_CLAMPED_NORMAL,
    INT_PROVIDER_WEIGHTED_LIST,
    INT_PROVIDER_INVALID = -1,
};

typedef struct int_or_provider {
    bool is_provider;
    union {
        i32 constant;
        struct int_provider* provider;
    } data;
} IntOrProvider;

typedef struct int_provider {
    enum IntProviderType type;
    union {
        i32 constant_value;
        struct minmax {
            i32 min_inclusive;
            i32 max_inclusive;
        } uniform_or_biased;
        struct {
            i32 min_inclusive;
            i32 max_inclusive;
            IntOrProvider source;
        } clamped;
        struct {
            i32 min_inclusive;
            i32 max_inclusive;
            f32 mean;
            f32 deviation;
        } clamped_normal;
        struct {
            struct weighted_entry {
                IntOrProvider data;
                i32 weight;
            }* entries;
            u64 entry_count;
        } weighted_list;
    } data;
} IntProvider;

typedef struct {
    i64 fixed_time;
    bool has_skylight;
    bool has_ceiling;
    bool ultrawarm;
    bool natural;
    f64 coordinate_scale;
    bool bed_works;
    bool respawn_anchor_works;
    i32 min_y;
    i32 height;
    i32 logical_height;
    ResourceID infiniburn;
    ResourceID effects;
    float ambient_light;
    bool piglin_safe;
    bool has_raids;
    IntOrProvider monster_spawn_light_level;
    i32 monster_spawn_block_light_limit;
} DimensionType;

#endif /* ! DIMENSION_TYPE_H */
