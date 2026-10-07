#include "memory/allocators/arena.h"
#include "registry/codec.h"
#include "world/data/damage_type.h"
#include "data/json.h"
#include "data/nbt.h"
#include "definitions.h"
#include "registry/codec/internal.h"

static const char* scaling_values[] = {
    [DMG_TYPE_SCALE_NEVER]                            = "never",
    [DMG_TYPE_SCALE_ALWAYS]                           = "always",
    [DMG_TYPE_SCALE_WHEN_CAUSED_BY_LIVING_NON_PLAYER] = "when_caused_by_living_non_player",
};
static u64 scaling_value_count      = sizeof scaling_values / sizeof scaling_values[0];
static const char* effects_values[] = {
    [DMG_TYPE_EFFECT_HURT]     = "hurt",
    [DMG_TYPE_EFFECT_THORNS]   = "thorns",
    [DMG_TYPE_EFFECT_DROWNING] = "drowning",
    [DMG_TYPE_EFFECT_BURNING]  = "burning",
    [DMG_TYPE_EFFECT_POKING]   = "poking",
    [DMG_TYPE_EFFECT_FREEZING] = "freezing",
};
static u64 effects_value_count = sizeof effects_values / sizeof effects_values[0];

static const char* death_message_type_values[] = {
    [DMG_TYPE_MSG_TYPE_DEFAULT]                 = "default",
    [DMG_TYPE_MSG_TYPE_FALL_VARIANTS]           = "fall_variants",
    [DMG_TYPE_MSG_TYPE_INTENTIONAL_GAME_DESIGN] = "intentional_game_design",
};
static u64 death_message_type_value_count =
    sizeof death_message_type_values / sizeof death_message_type_values[0];

DEFINE_FROM_JSON(DamageType, damage_type) {
    DamageType result = {};

    json_easy_get_float(json, result, exhaustion);

    result.message_id = json_easy_get_string(json, "message_id", persistent_arena);

    result.scaling =
        json_easy_get_enum(json, "scaling", scaling_values, scaling_value_count, scratch_arena);
    result.effects =
        json_easy_get_enum(json, "effects", effects_values, effects_value_count, scratch_arena);
    result.death_message_type = json_easy_get_enum(json,
                                                   "death_message_type",
                                                   death_message_type_values,
                                                   death_message_type_value_count,
                                                   scratch_arena);

    return result;
}

DEFINE_TO_NBT(DamageType, damage_type) {

    UNUSED(scratch_arena);
    UNUSED(persistent_arena);

    *out_nbt = nbt_create(persistent_arena, 32);

    nbt_easy_str(out_nbt, obj, message_id)
    nbt_easy_float(out_nbt, obj, exhaustion);
    nbt_easy_str_2(out_nbt, "scaling", scaling_values[obj->scaling]);
    nbt_easy_str_2(out_nbt, "effects", effects_values[obj->effects]);
    nbt_easy_str_2(out_nbt, "death_message_type", death_message_type_values[obj->death_message_type]);
}
