#include "codec.h"
#include "data/nbt.h"
#include "definitions.h"
#include "utils/string.h"
#include "world/data/dimension_type.h"

#define get_bool(json, obj, name)                                                                  \
    json_move_to_cstr(json, #name);                                                                \
    (obj).name = json_get_bool(json);                                                              \
    json_move_to_parent(json);

#define get_int(json, obj, name)                                                                   \
    json_move_to_cstr(json, #name);                                                                \
    (obj).name = json_get_int(json);                                                               \
    json_move_to_parent(json);

#define nbt_set_bool(nbt, obj, name)                                                               \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(nbt, &tmp_str, NBT_BYTE, (union NBTSimpleValue) {.byte = ((obj)->name)});    \
    }

DimensionType dimension_type_from_json(JSON* json, Arena scratch_arena, Arena* persistent_arena) {
    UNUSED(scratch_arena);
    DimensionType new_type;

    if (json_move_to_cstr(json, "fixed_time") == JSONE_OK) {
        new_type.fixed_time = json_get_int(json);
        json_move_to_parent(json);
    }

    json_move_to_parent(json);

    get_bool(json, new_type, has_skylight);
    get_bool(json, new_type, has_ceiling);
    get_bool(json, new_type, ultrawarm);
    get_bool(json, new_type, natural);

    json_move_to_cstr(json, "coordinate_scale");
    new_type.coordinate_scale = json_get_float(json);
    json_move_to_parent(json);

    get_bool(json, new_type, bed_works);
    get_bool(json, new_type, respawn_anchor_works);
    get_int(json, new_type, min_y);
    get_int(json, new_type, height);
    get_int(json, new_type, logical_height);

    get_bool(json, new_type, piglin_safe);
    get_bool(json, new_type, has_raids);
    get_int(json, new_type, monster_spawn_block_light_limit);

    json_move_to_cstr(json, "infiniburn");
    string* tmp = json_get_string(json); // Allocated with `scratch`, in json data
    resid_parse(tmp, persistent_arena, &new_type.infiniburn);

    return new_type;
}

void dimension_type_to_nbt(const DimensionType* obj,
                           Arena scratch_arena,
                           Arena* persistent_arena,
                           NBT* out_nbt) {
    UNUSED(scratch_arena);
    *out_nbt = nbt_create(persistent_arena, 32);

    nbt_set_bool(out_nbt, obj, has_skylight);
    nbt_set_bool(out_nbt, obj, has_ceiling);
    nbt_set_bool(out_nbt, obj, ultrawarm);
    nbt_set_bool(out_nbt, obj, natural);
    nbt_set_bool(out_nbt, obj, bed_works);
    nbt_set_bool(out_nbt, obj, respawn_anchor_works);
    nbt_set_bool(out_nbt, obj, piglin_safe);
    nbt_set_bool(out_nbt, obj, has_raids);
}
