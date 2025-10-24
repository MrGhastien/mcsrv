#ifndef DIMENSION_TYPE_H
#define DIMENSION_TYPE_H

#include "resource/resource_id.h"
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
    bool piglin_safe;
    bool has_raids;
    i32 monster_spawn_light_level;
    i32 monster_spawn_block_light_limit;
} DimensionType;    

#endif /* ! DIMENSION_TYPE_H */
