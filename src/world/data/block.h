//
// Created by bmorino on 10/01/2025.
//

#ifndef BLOCK_H
#define BLOCK_H

#include "containers/vector.h"
#include "definitions.h"

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
        };
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
    const BlockState* states;
} StateDefinition;

// Forward typedef, see at the beginning of the file
struct block_state {
    const StateDefinition* definition;
    union StatePropertyValue* values;
};

// Forward typedef, see at the beginning of the file
struct block {
    ResourceID id;
    f32 destroy_time;
    f32 explosion_resistance;
    i32 light_level;
    f32 slipperiness;
    // TODO: other attributes
    StateDefinition state_definition;
};

StateProperty create_integer_state_property(string name, i32 minimum, i32 maximum);
StateProperty create_bool_state_property(string name);
StateProperty create_enum_state_property(string name, Vector* values);

StateDefinition create_state_definition(const Block* block, Vector* properties);

const BlockState* state_any(const StateDefinition* definition);
const BlockState* state_with_value(const BlockState* state, const StateProperty* property, union StatePropertyValue value);

#endif /* ! BLOCK_H */
