//
// Created by bmorino on 10/01/2025.
//

#ifndef BLOCK_H
#define BLOCK_H

#include "block_behavior.h"
#include "containers/vector.h"
#include "definitions.h"
#include "resource/resource_id.h"
#include "utils/string.h"

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
typedef bool (*context_predicate)(const BlockState* state, i32 world_view, i32 block_pos);

typedef struct block_properties {
    f32 destroy_time;
    f32 explosion_resistance;
    f32 slipperiness;
    f32 speed_multiplier;
    f32 jump_multiplier;
    bool ticks_randomly         : 1;
    bool requires_correct_tools : 1;
    bool has_collision          : 1;
    bool opaque                 : 1;
    bool ignited_by_lava        : 1;
    bool
        spawn_brushing_particles : 1; // Used by the brush item logic to not draw brushing particles
    bool replaceable             : 1;
    bool has_dynamic_shape       : 1;

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

typedef struct state_selection_ctx {
    const Block* block;
    i64* value_indices;
} StateSelectionContext;

i64 register_integer_state_property(string name, i32 minimum, i32 maximum);
i64 register_bool_state_property(string name);
i64 register_enum_state_property(string name, string* values, u64 value_count);

bool create_state_definition(const Block* block,
                             Vector* properties,
                             Arena* arena,
                             StateDefinition* out_definition);

BlockProperties default_block_properties(void);

const BlockState* state_any(const StateDefinition* definition);
const BlockState* state_with_value(const BlockState* state,
                                   const StateProperty* property,
                                   union StatePropertyValue value);

const StateProperty* get_state_property_by_name(const Block* block, string property_name);
union StatePropertyValue parse_state_property_value(string value, const StateProperty* prop);

bool selector_init(StateSelectionContext* out_ctx, Arena* arena, ResourceID block_id);
void selector_set(StateSelectionContext* ctx,
                  const StateProperty* property,
                  union StatePropertyValue value);
const BlockState* selector_select(const StateSelectionContext* ctx);

#endif /* ! BLOCK_H */
