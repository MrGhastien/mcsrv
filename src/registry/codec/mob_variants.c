#include "world/data/mob_variants.h"
#include "data/json.h"
#include "data/nbt.h"
#include "definitions.h"
#include "logger.h"
#include "memory/allocators/arena.h"
#include "platform/platform.h"
#include "registry/codec.h"
#include "registry/codec/internal.h"
#include "resource/resource_id.h"
#include "utils/string.h"

static WolfVariantAssets
wolf_variant_assets_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena) {
    WolfVariantAssets result = {};

    string angry = json_easy_get_string(json, "angry_texture", scratch_arena);
    string wild  = json_easy_get_string(json, "wild_texture", scratch_arena);
    string tame  = json_easy_get_string(json, "tame_texture", scratch_arena);

    if (!angry.base || !wild.base || !tame.base)
        return result;

    if (!resid_parse(angry, persistent_arena, &result.angry) ||
        !resid_parse(wild, persistent_arena, &result.wild) ||
        !resid_parse(tame, persistent_arena, &result.tame)) {
        return (WolfVariantAssets){};
    }

    return result;
}

static void wolf_variant_assets_to_nbt(const WolfVariantAssets* obj,
                                       Arena* scratch_arena,
                                       Arena* persistent_arena,
                                       NBT* out_nbt) {
    UNUSED(persistent_arena);

    nbt_put_str(out_nbt, STR_LITERAL("angry_texture"), resid_to_string(&obj->angry, scratch_arena));
    nbt_put_str(out_nbt, STR_LITERAL("wild_texture"), resid_to_string(&obj->wild, scratch_arena));
    nbt_put_str(out_nbt, STR_LITERAL("tame_texture"), resid_to_string(&obj->tame, scratch_arena));
}

static const char* spawn_condition_type_values[] = {
    [SPAWN_CONDITION_ALWAYS]          = "",
    [SPAWN_CONDITION_BIOME]           = "biome",
    [SPAWN_CONDITION_STRUCTURE]       = "structure",
    [SPAWN_CONDITION_MOON_BRIGHTNESS] = "moon_brightness",
};
static u64 spawn_condition_type_value_count =
    sizeof spawn_condition_type_values / sizeof spawn_condition_type_values[0];

static IDOrTag id_or_tag_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena) {
    UNUSED(scratch_arena);
    IDOrTag result = {};
    string* s      = json_get_string(json);
    ResourceID id;
    platform_assert(resid_parse(*s, persistent_arena, &id), "Failed to parse resid");
    bool is_tag = str_find_char(id.namespace, '#') == 0;
    if (is_tag)
        result.contents.tag = id;
    else
        result.contents.id = id;
    result.is_tag = is_tag;

    return result;
}

static string id_or_tag_to_string(const IDOrTag* id_or_tag, Arena* scratch_arena) {
    ResourceID id = id_or_tag->is_tag ? id_or_tag->contents.tag : id_or_tag->contents.id;
    return resid_to_string(&id, scratch_arena);
}

static IDOrTagList
id_or_tag_list_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena) {
    if (json_get_type(json) == JSON_STRING) {
        IDOrTag* elements = arena_callocate(persistent_arena, sizeof *elements * 1);
        elements[0]       = id_or_tag_from_json(json, scratch_arena, persistent_arena);
        return (IDOrTagList){
            .elements      = elements,
            .element_count = 1,
        };
    }

    i64 len = json_get_length(json);

    IDOrTag* elements = arena_callocate(persistent_arena, sizeof *elements * len);

    i64 index = 0;
    json_move_to_index(json, 0);
    while (json_move_to_next_sibling(json) == JSONE_OK) {
        elements[index] = id_or_tag_from_json(json, scratch_arena, persistent_arena);
        index++;
    }

    return (IDOrTagList){
        .elements      = elements,
        .element_count = len,
    };
}

static SpawnCondition
spawn_condition_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena) {
    SpawnCondition result = {
        .type = SPAWN_CONDITION_ALWAYS,
    };

    json_easy_get_int(json, result, priority);

    if (json_move_to_cstr(json, "condition") == JSONE_OK) {
        u64 type_enum = json_easy_get_enum(json,
                                           "type",
                                           spawn_condition_type_values,
                                           spawn_condition_type_value_count,
                                           scratch_arena);
        if (type_enum < spawn_condition_type_value_count)
            result.type = type_enum;

        switch (result.type) {
        case SPAWN_CONDITION_BIOME:
            json_move_to_cstr(json, "biomes");
            result.data.biomes_or_structures = id_or_tag_list_from_json(json, scratch_arena, persistent_arena);
            json_move_to_parent(json);
            break;
        case SPAWN_CONDITION_STRUCTURE:
            json_move_to_cstr(json, "structures");
            result.data.biomes_or_structures = id_or_tag_list_from_json(json, scratch_arena, persistent_arena);
            json_move_to_parent(json);
            break;
        case SPAWN_CONDITION_MOON_BRIGHTNESS:
            json_move_to_cstr(json, "range");
            enum JSONType json_type = json_get_type(json);
            switch (json_type) {
            case JSON_FLOAT:
                result.data.moon_brightness.min = json_get_float(json);
                result.data.moon_brightness.max = result.data.moon_brightness.min;
                break;
            case JSON_OBJECT:
                json_move_to_cstr(json, "min");
                result.data.moon_brightness.min = json_get_float(json);
                json_move_to_parent(json);

                json_move_to_cstr(json, "max");
                result.data.moon_brightness.max = json_get_float(json);
                json_move_to_parent(json);
                break;
            default:
                platform_abort();
                break;
            }
            break;
        default:
            platform_abort();
        }
    }

    return result;
}

static SpawnCondition* spawn_conditions_from_json(JSON* json,
                                                  Arena* scratch_arena,
                                                  Arena* persistent_arena,
                                                  u64* out_len) {
    i64 len = json_get_length(json);

    SpawnCondition* conditions = arena_callocate(persistent_arena, sizeof *conditions * len);

    i64 index = 0;
    json_move_to_index(json, 0);
    while (json_move_to_next_sibling(json) == JSONE_OK) {
        conditions[index] = spawn_condition_from_json(json, scratch_arena, persistent_arena);
        index++;
    }

    *out_len = len;
    return conditions;
}

static void spawn_conditions_to_nbt(const SpawnCondition* conditions,
                                    u64 condition_count,
                                    Arena* scratch_arena,
                                    Arena* persistent_arena,
                                    NBT* out_nbt) {
    UNUSED(persistent_arena);
    for (u64 i = 0; i < condition_count; i++) {
        nbt_push(out_nbt, NBT_COMPOUND);
        const SpawnCondition* condition = &conditions[i];
        nbt_easy_int(out_nbt, condition, priority);
        enum SpawnConditionType type = condition->type;
        if (type != SPAWN_CONDITION_ALWAYS) {
            nbt_put(out_nbt, STR_LITERAL("condition"), NBT_COMPOUND);
            nbt_put_str(out_nbt, STR_LITERAL("type"), str_view(spawn_condition_type_values[type]));
            switch (type) {
            case SPAWN_CONDITION_BIOME:
            case SPAWN_CONDITION_STRUCTURE: {
                string field_name =
                    STR_LITERAL(type == SPAWN_CONDITION_BIOME ? "biomes" : "structures");
                if (condition->data.biomes_or_structures.element_count == 1) {
                    nbt_put_str(
                        out_nbt,
                        field_name,
                        id_or_tag_to_string(&condition->data.biomes_or_structures.elements[0],
                                            scratch_arena));
                } else {
                    nbt_put(out_nbt, field_name, NBT_LIST);

                    for (u64 j = 0; j < condition->data.biomes_or_structures.element_count; j++) {
                        nbt_put_str(
                            out_nbt,
                            field_name,
                            id_or_tag_to_string(&condition->data.biomes_or_structures.elements[j],
                                                scratch_arena));
                    }

                    nbt_move_to_parent(out_nbt);
                }
                break;
            }
            case SPAWN_CONDITION_MOON_BRIGHTNESS: {
                f64 min = condition->data.moon_brightness.min;
                f64 max = condition->data.moon_brightness.max;
                if (min == max) {
                    nbt_put_simple(out_nbt,
                                   STR_LITERAL("range"),
                                   NBT_DOUBLE,
                                   (union NBTSimpleValue){.double_num = min});
                } else {
                    nbt_put(out_nbt, STR_LITERAL("range"), NBT_COMPOUND);
                    nbt_put_simple(out_nbt,
                                   STR_LITERAL("min"),
                                   NBT_DOUBLE,
                                   (union NBTSimpleValue){.double_num = min});
                    nbt_put_simple(out_nbt,
                                   STR_LITERAL("max"),
                                   NBT_DOUBLE,
                                   (union NBTSimpleValue){.double_num = max});
                    nbt_move_to_parent(out_nbt);
                }
                break;
            }
            default:
                platform_abort();
                break;
            }
            nbt_move_to_parent(out_nbt);
        }
        nbt_move_to_parent(out_nbt);
    }
}

DEFINE_FROM_JSON(WolfVariant, wolf_variant) {
    WolfVariant result = {};

    /* if (json_move_to_cstr(json, "assets") != JSONE_OK) { */
    /*     return (WolfVariant){}; */
    /* } */
    result.assets = wolf_variant_assets_from_json(json, scratch_arena, persistent_arena);
    //json_move_to_parent(json);

    /*
    if (json_move_to_cstr(json, "baby_assets") != JSONE_OK) {
        return (WolfVariant){};
    }
    result.baby_assets = wolf_variant_assets_from_json(json, scratch_arena, persistent_arena);
    json_move_to_parent(json);
    */

    if(json_move_to_cstr(json, "spawn_conditions") == JSONE_OK) {
        result.conditions =
            spawn_conditions_from_json(json, scratch_arena, persistent_arena, &result.condition_count);
        json_move_to_parent(json);
    }

    return result;
}

DEFINE_TO_NBT(WolfVariant, wolf_variant) {

    UNUSED(scratch_arena);
    *out_nbt = nbt_create(persistent_arena, 32);

    //nbt_put(out_nbt, STR_LITERAL("assets"), NBT_COMPOUND);
    //nbt_move_to_name(out_nbt, STR_LITERAL("assets"));
    wolf_variant_assets_to_nbt(&obj->assets, scratch_arena, persistent_arena, out_nbt);
    //nbt_move_to_parent(out_nbt);

    /*
    nbt_put(out_nbt, STR_LITERAL("baby_assets"), NBT_COMPOUND);
    nbt_move_to_name(out_nbt, STR_LITERAL("baby_assets"));
    wolf_variant_assets_to_nbt(&obj->baby_assets, scratch_arena, persistent_arena, out_nbt);
    nbt_move_to_parent(out_nbt);
    */

    nbt_move_to_name(out_nbt, STR_LITERAL("spawn_conditions"));
    nbt_put(out_nbt, STR_LITERAL("spawn_conditions"), NBT_LIST);
    spawn_conditions_to_nbt(
        obj->conditions, obj->condition_count, scratch_arena, persistent_arena, out_nbt);
    nbt_move_to_parent(out_nbt);
}
