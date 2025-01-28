//
// Created by bmorino on 10/01/2025.
//

#ifndef BLOCK_H
#define BLOCK_H

#include "containers/vector.h"
#include "definitions.h"
#include "utils/string.h"
#include "resource/resource_id.h"
#include "block_behavior.h"

typedef struct block Block;
typedef struct block_state BlockState;

enum BlockStatePropertyType {
    BLOCK_PROP_INTEGER,
    BLOCK_PROP_BOOL,
    BLOCK_PROP_ENUM,
    _BLOCK_PROP_COUNT
};

typedef struct state_property {
    enum BlockStatePropertyType type;
    string name;
    union info {
        struct integer_property {
            i32 min;
            i32 max;
        } integer;
        struct enum_property {
            u32 value_count;
            string* values;
        } enumeration;
    } info;
} StateProperty;

union StatePropertyValue {
    i32 integer;
    bool boolean;
    u32 enum_index;
};

typedef struct state_definition {
    u32 property_count;
    u32 state_count;
    const StateProperty** properties;
    BlockState* states;
} StateDefinition;

// Forward typedef, see at the beginning of the file
struct block_state {
    const StateDefinition* definition;
    union StatePropertyValue* values;
};

enum MapColor {
    MAP_COLOR_RED,
};

enum PistonBehavior {
	PISTON_BEHAVIOR_NORMAL,
	PISTON_BEHAVIOR_DESTROY,
	PISTON_BEHAVIOR_BLOCK,
	PISTON_BEHAVIOR_IGNORE,
	PISTON_BEHAVIOR_PUSH_ONLY,
};

typedef i32 (*light_function)(const BlockState* state);
typedef enum MapColor (*map_color_function)(const BlockState* state);
typedef bool (*context_predicate) (const BlockState* state, i32 world_view, i32 block_pos);

typedef struct block_properties {
    f32 destroy_time;
    f32 explosion_resistance;
    f32 slipperiness;
    f32 speed_multiplier;
    f32 jump_multiplier;
    bool ticks_randomly;
    bool requires_correct_tools;
    bool has_collision;
    bool opaque;
    bool ignited_by_lava;
    bool spawn_brushing_particles; // Used by the brush item logic to not draw brushing particles
    bool replaceable;
    bool has_dynamic_shape;

    light_function light_getter;
    map_color_function map_color_getter;

    context_predicate spawn_predicate;
    context_predicate redstone_conduction_predicate;
    context_predicate suffocation_predicate;
    context_predicate block_vision_predicate;
    context_predicate post_processing_predicate;
    context_predicate emissive_predicate;

    // TODO: Feature flags

    enum PistonBehavior piston_behavior;

} BlockProperties;

// Forward typedef, see at the beginning of the file
struct block {
    ResourceID id;
    BlockProperties properties;
    BlockBehavior behavior;
    // TODO: other attributes
    StateDefinition state_definition;
};

i64 register_integer_state_property(string name, i32 minimum, i32 maximum);
i64 register_bool_state_property(string name);
i64 register_enum_state_property(string name, Arena* arena, string* values, u64 value_count);

bool create_state_definition(const Block* block, Vector* properties, Arena* arena, StateDefinition* out_definition);

BlockProperties default_block_properties(void);

const BlockState* state_any(const StateDefinition* definition);
const BlockState* state_with_value(const BlockState* state, const StateProperty* property, union StatePropertyValue value);

#endif /* ! BLOCK_H */
