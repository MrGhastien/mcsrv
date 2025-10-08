#include "block.h"
#include "containers/vector.h"
#include "logger.h"
#include "memory/allocators/arena.h"
#include "memory/mem_tags.h"
#include "registry/registry.h"
#include "resource/resource_id.h"
#include "utils/string.h"
#include <stddef.h>
#include <stdlib.h>

static u32 get_value_count(const StateProperty* property) {
    switch (property->type) {
    case BLOCK_PROP_ENUM:
        return property->info.enumeration.value_count;
    case BLOCK_PROP_BOOL:
        return 2;
    case BLOCK_PROP_INTEGER:
        return property->info.integer.max - property->info.integer.min + 1;
    default:
        return 0;
    }
}

static union StatePropertyValue get_value(const StateProperty* property, u32 index) {
    u32 value_count = get_value_count(property);
    if (index >= value_count) {
        log_warnf("Invalid property value index: %u, max: %u", index, value_count - 1);
        index = 0;
    }
    union StatePropertyValue val = {0};
    switch (property->type) {
    case BLOCK_PROP_BOOL:
        val.boolean = index == 1 ? TRUE : FALSE;
        break;
    case BLOCK_PROP_INTEGER:
        val.integer = index + property->info.integer.min;
        break;
    case BLOCK_PROP_ENUM:
        val.enum_index = index;
        break;
    default:
        break;
    }
    return val;
}

bool create_state_definition(const Block* block,
                             Vector* properties,
                             Arena* arena,
                             StateDefinition* out_definition) {
    if (!out_definition || !block || !arena)
        return FALSE;

    u32 property_count = properties ? vect_size(properties) : 0;
    if (property_count == 0) {
        out_definition->property_count = 0;
        out_definition->properties     = NULL;
        out_definition->state_count    = 1;
        out_definition->states  = arena_callocate(arena, sizeof(BlockState) /*, ALLOC_TAG_WORLD */);
        *out_definition->states = (BlockState) {
            .definition = out_definition,
            .values     = NULL,
        };
        return TRUE;
    }

    const StateProperty** property_array =
        arena_callocate(arena, sizeof(StateProperty*) * property_count /*, ALLOC_TAG_WORLD */);

    u32 state_count = 1;

    for (u32 i = 0; i < property_count; i++) {
        // This is fine, as we write a pointer (not the actual property structure).
        vect_get(properties, i, &property_array[i]);
        const StateProperty* prop = property_array[i];

        // Check if no other property has the same name
        for (u32 j = 0; j < i; j++) {
            const StateProperty* prop2 = property_array[j];
            if (str_compare(&prop->name, &prop2->name) == 0) {
                log_errorf("Property %s already exists in block %s:%s.",
                           cstr(&prop->name),
                           cstr(&block->id.namespace),
                           cstr(&block->id.path));
                arena_free_ptr(arena, property_array);
                return FALSE;
            }
        }

        state_count *= get_value_count(prop);
    }

    BlockState* states =
        arena_callocate(arena, sizeof *states * state_count /*, ALLOC_TAG_WORLD */);

    for (u32 i = 0; i < state_count; i++) {
        union StatePropertyValue* values =
            arena_callocate(arena, sizeof *values * property_count /* , ALLOC_TAG_WORLD */);

        u32 val_index = 1;
        for (i32 j = property_count - 1; j >= 0; j--) {
            const StateProperty* prop = property_array[j];
            u32 local_value_count     = get_value_count(prop);
            values[j]                 = get_value(prop, (i / val_index) % local_value_count);
            val_index *= local_value_count;
        }

        states[i] = (BlockState) {
            .definition = out_definition,
            .values     = values,
        };
    }

    *out_definition = (StateDefinition) {
        .property_count = property_count,
        .properties     = property_array,
        .state_count    = state_count,
        .states         = states,
    };

    return TRUE;
}

BlockProperties default_block_properties(void) {
    return (BlockProperties) {
        .slipperiness             = 0.6f,
        .speed_multiplier         = 1.0f,
        .jump_multiplier          = 1.0f,
        .opaque                   = TRUE,
        .spawn_brushing_particles = TRUE,
        .piston_behavior          = PISTON_BEHAVIOR_NORMAL,
        .requires_correct_tools   = TRUE,
        .destroy_time             = 1.5f,
        .explosion_resistance     = 6.0f,
    };
}

const BlockState* state_any(const StateDefinition* definition);
const BlockState* state_with_value(const BlockState* state,
                                   const StateProperty* property,
                                   union StatePropertyValue value);

bool selector_init(StateSelectionContext* out_ctx, Arena* arena, ResourceID block_id) {
    const Block* block = registry_get(resid_default_cstr("blocks"), block_id);
    if (!block)
        return FALSE;
    out_ctx->block = block;
    if (block->state_definition.property_count == 0)
        return TRUE;

    out_ctx->value_indices =
        arena_allocate(arena,
                       sizeof *out_ctx->value_indices * block->state_definition.property_count/* ,
                       ALLOC_TAG_WORLD */);
    if (!out_ctx->value_indices)
        return FALSE;

    for (u32 i = 0; i < block->state_definition.property_count; i++) {
        out_ctx->value_indices[i] = -1;
    }

    return TRUE;
}
void selector_set(StateSelectionContext* ctx,
                  const StateProperty* property,
                  union StatePropertyValue value) {
    const StateDefinition* def = &ctx->block->state_definition;
    u32 i                      = 0;
    while (i < def->property_count && def->properties[i] != property) {
        i++;
    }
    if (i == def->property_count)
        return;

    i64 value_idx = -1;
    switch (property->type) {
    case BLOCK_PROP_BOOL:
        value_idx = value.boolean ? 1 : 0;
        break;
    case BLOCK_PROP_INTEGER:
        if (value.integer <= property->info.integer.max)
            value_idx = value.integer - property->info.integer.min;
        break;
    case BLOCK_PROP_ENUM:
        value_idx = value.enum_index;
        break;
    default:
        break;
    }

    ctx->value_indices[i] = value_idx;
}
const BlockState* selector_select(const StateSelectionContext* ctx) {
    i64 final_idx = 0;

    if (ctx->block->state_definition.property_count == 0)
        return &ctx->block->state_definition.states[0];

    for (i32 i = ctx->block->state_definition.property_count - 1; i >= 0; i--) {
        i64 val_idx = ctx->value_indices[i];
        if (val_idx < 0)
            return NULL;

        final_idx += val_idx * (i + 1);
    }

    return &ctx->block->state_definition.states[final_idx];
}

union StatePropertyValue parse_state_property_value(string value, const StateProperty* prop) {
    union StatePropertyValue res;
    switch (prop->type) {
    case BLOCK_PROP_BOOL:
        if (str_compare_cstr(&value, "true") == 0)
            res.boolean = TRUE;
        else if (str_compare_cstr(&value, "false") == 0)
            res.boolean = FALSE;
        else
            abort();
        break;
    case BLOCK_PROP_INTEGER:
        i64 num     = strtol(cstr(&value), NULL, 10);
        res.integer = num;
        break;
    case BLOCK_PROP_ENUM:
        u32 i;
        for (i = 0; i < prop->info.enumeration.value_count; i++) {
            string* possible_value = &prop->info.enumeration.values[i];
            if (str_compare(possible_value, &value) == 0)
                break;
        }
        res.enum_index = i;
        break;
    default:
        res.integer = -1;
        break;
    }
    return res;
}
