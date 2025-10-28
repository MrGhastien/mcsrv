#include "codec.h"
#include "data/json.h"
#include "data/nbt.h"
#include "definitions.h"
#include "memory/allocators/arena.h"
#include "platform/platform.h"
#include "resource/resource_id.h"
#include "utils/string.h"
#include "world/data/dimension_type.h"

#define json_easy_get_bool(json, obj, name)                                                        \
    {                                                                                              \
        json_move_to_cstr(json, #name);                                                            \
        (obj).name = json_get_bool(json);                                                          \
        json_move_to_parent(json);                                                                 \
    }

#define json_easy_get_int(json, obj, name)                                                         \
    {                                                                                              \
        json_move_to_cstr(json, #name);                                                            \
        (obj).name = json_get_int(json);                                                           \
        json_move_to_parent(json);                                                                 \
    }

#define json_easy_get_float(json, obj, name)                                                       \
    {                                                                                              \
        json_move_to_cstr(json, #name);                                                            \
        (obj).name = json_get_float(json);                                                         \
        json_move_to_parent(json);                                                                 \
    }

#define nbt_easy_bool(nbt, obj, name)                                                              \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(nbt, &tmp_str, NBT_BYTE, (union NBTSimpleValue) {.byte = ((obj)->name)});   \
    }

#define nbt_easy_int(nbt, obj, name)                                                               \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(nbt, &tmp_str, NBT_INT, (union NBTSimpleValue) {.integer = ((obj)->name)}); \
    }

#define nbt_easy_long(nbt, obj, name)                                                              \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(                                                                            \
            nbt, &tmp_str, NBT_LONG, (union NBTSimpleValue) {.long_num = ((obj)->name)});          \
    }

#define nbt_easy_double(nbt, obj, name)                                                            \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(                                                                            \
            nbt, &tmp_str, NBT_DOUBLE, (union NBTSimpleValue) {.double_num = ((obj)->name)});      \
    }

#define nbt_easy_float(nbt, obj, name)                                                             \
    {                                                                                              \
        string tmp_str = str_view(#name);                                                          \
        nbt_put_simple(                                                                            \
            nbt, &tmp_str, NBT_FLOAT, (union NBTSimpleValue) {.float_num = ((obj)->name)});        \
    }

static const string int_provider_type_names[] = {
    [INT_PROVIDER_CONSTANT]         = STR_STATIC("constant"),
    [INT_PROVIDER_UNIFORM]          = STR_STATIC("uniform"),
    [INT_PROVIDER_BIASED_TO_BOTTOM] = STR_STATIC("biased_to_bottom"),
    [INT_PROVIDER_CLAMPED]          = STR_STATIC("clamped"),
    [INT_PROVIDER_CLAMPED_NORMAL]   = STR_STATIC("clamped_normal"),
    [INT_PROVIDER_WEIGHTED_LIST]    = STR_STATIC("weighted_list"),
};

static enum IntProviderType int_provider_parse_type(const string* str) {
    static const size_t type_count =
        sizeof int_provider_type_names / sizeof(*int_provider_type_names);

    for (size_t i = 0; i < type_count; i++) {
        if (str_compare(str, &int_provider_type_names[i]) == 0)
            return i;
    }
    return INT_PROVIDER_INVALID;
}

static IntOrProvider
int_provider_from_json(JSON* json, Arena scratch_arena, Arena* persistent_arena) {

    enum JSONType json_type = json_get_type(json);
    if (json_type == JSON_INT) {
        return (IntOrProvider) {
            .is_provider   = FALSE,
            .data.constant = json_get_int(json),
        };
    }

    json_move_to_cstr(json, "type");
    string* type_name = json_get_string(json);
    json_move_to_parent(json);

    enum IntProviderType type = int_provider_parse_type(type_name);
    if (type == INT_PROVIDER_INVALID) {
        return (IntOrProvider) {
            .is_provider = TRUE,
        };
    }

    IntProvider* provider = arena_allocate(persistent_arena, sizeof *provider);
    provider->type        = type;

    switch (type) {
    case INT_PROVIDER_CONSTANT:
        json_move_to_cstr(json, "value");
        provider->data.constant_value = json_get_int(json);
        json_move_to_parent(json);
        json_easy_get_int(json, provider->data, constant_value);
        break;
    case INT_PROVIDER_UNIFORM:
    case INT_PROVIDER_BIASED_TO_BOTTOM:
        json_easy_get_int(json, provider->data.uniform_or_biased, min_inclusive);
        json_easy_get_int(json, provider->data.uniform_or_biased, max_inclusive);
        break;
    case INT_PROVIDER_CLAMPED:
        json_easy_get_int(json, provider->data.clamped, min_inclusive);
        json_easy_get_int(json, provider->data.clamped, max_inclusive);
        provider->data.clamped.source =
            int_provider_from_json(json, scratch_arena, persistent_arena);
        break;
    case INT_PROVIDER_CLAMPED_NORMAL:
        json_easy_get_int(json, provider->data.clamped_normal, min_inclusive);
        json_easy_get_int(json, provider->data.clamped_normal, max_inclusive);
        json_easy_get_float(json, provider->data.clamped_normal, mean);
        json_easy_get_float(json, provider->data.clamped_normal, deviation);
        break;
    case INT_PROVIDER_WEIGHTED_LIST:
        json_move_to_cstr(json, "distribution");
        size_t length = json_get_length(json);
        provider->data.weighted_list.entry_count = length;
        if (length == 0) {
            provider->data.weighted_list.entries = NULL;
            break;
        }

        provider->data.weighted_list.entries =
            arena_allocate(persistent_arena, length * sizeof *provider->data.weighted_list.entries);
        json_move_to_index(json, 0);
        size_t idx = 0;
        do {
            struct weighted_entry* entry = &provider->data.weighted_list.entries[idx];
            entry->data = int_provider_from_json(json, scratch_arena, persistent_arena);
            json_easy_get_int(json, *entry, weight);
            idx++;
        } while (json_move_to_next_sibling(json) == JSONE_OK);
        json_move_to_parent(json);
        json_move_to_parent(json);
        break;
    default:
        platform_abort();
        break;
    }

    IntOrProvider ret = {.is_provider = TRUE, .data.provider = provider};

    return ret;
}

static void int_provider_to_nbt(
    NBT* nbt, const IntOrProvider* integer, string name, Arena scratch_arena, Arena* persistent_arena) {

    if (!integer->is_provider) {
        nbt_put_simple(
            nbt, &name, NBT_INT, (union NBTSimpleValue) {.integer = integer->data.constant});
        return;
    }

    IntProvider* provider = integer->data.provider;

    nbt_put(nbt, &name, NBT_COMPOUND);

    string tmp_str = str_view("type");
    nbt_put_str(nbt, &tmp_str, &int_provider_type_names[provider->type]);

    switch (provider->type) {
    case INT_PROVIDER_CONSTANT:
        tmp_str = str_view("value");
        nbt_put_simple(nbt,
                       &tmp_str,
                       NBT_INT,
                       (union NBTSimpleValue) {.integer = provider->data.constant_value});
        break;
    case INT_PROVIDER_UNIFORM:
    case INT_PROVIDER_BIASED_TO_BOTTOM:
        tmp_str = str_view("min_inclusive");
        nbt_put_simple(
            nbt,
            &tmp_str,
            NBT_INT,
            (union NBTSimpleValue) {.integer = provider->data.uniform_or_biased.min_inclusive});
        tmp_str = str_view("max_inclusive");
        nbt_put_simple(
            nbt,
            &tmp_str,
            NBT_INT,
            (union NBTSimpleValue) {.integer = provider->data.uniform_or_biased.max_inclusive});
        break;
    case INT_PROVIDER_CLAMPED:
        tmp_str = str_view("min_inclusive");
        nbt_put_simple(nbt,
                       &tmp_str,
                       NBT_INT,
                       (union NBTSimpleValue) {.integer = provider->data.clamped.min_inclusive});
        tmp_str = str_view("max_inclusive");
        nbt_put_simple(nbt,
                       &tmp_str,
                       NBT_INT,
                       (union NBTSimpleValue) {.integer = provider->data.clamped.max_inclusive});
        tmp_str = str_view("source");
        int_provider_to_nbt(
            nbt, &provider->data.clamped.source, tmp_str, scratch_arena, persistent_arena);
        break;
    case INT_PROVIDER_CLAMPED_NORMAL:
        tmp_str = str_view("min_inclusive");
        nbt_put_simple(
            nbt,
            &tmp_str,
            NBT_INT,
            (union NBTSimpleValue) {.integer = provider->data.clamped_normal.min_inclusive});
        tmp_str = str_view("max_inclusive");
        nbt_put_simple(
            nbt,
            &tmp_str,
            NBT_INT,
            (union NBTSimpleValue) {.integer = provider->data.clamped_normal.max_inclusive});
        tmp_str = str_view("mean");
        nbt_put_simple(nbt,
                       &tmp_str,
                       NBT_FLOAT,
                       (union NBTSimpleValue) {.float_num = provider->data.clamped_normal.mean});
        tmp_str = str_view("deviation");
        nbt_put_simple(
            nbt,
            &tmp_str,
            NBT_FLOAT,
            (union NBTSimpleValue) {.float_num = provider->data.clamped_normal.deviation});
        break;
    case INT_PROVIDER_WEIGHTED_LIST:
        tmp_str = (string) STR_STATIC("distribution");
        nbt_put(nbt, &tmp_str, NBT_LIST);

        for (u64 i = 0; i < provider->data.weighted_list.entry_count; i++) {
            nbt_push(nbt, NBT_COMPOUND);
            tmp_str = (string) STR_STATIC("weight");
            nbt_put_simple(
                nbt,
                &tmp_str,
                NBT_INT,
                (union NBTSimpleValue) {.integer = provider->data.weighted_list.entries[i].weight});
            tmp_str = (string) STR_STATIC("data");
            int_provider_to_nbt(nbt,
                                &provider->data.weighted_list.entries[i].data,
                                (string) STR_STATIC("data"),
                                scratch_arena,
                                persistent_arena);
            nbt_move_to_parent(nbt);
        }
        nbt_move_to_parent(nbt);
        break;
    default:
        platform_abort();
        break;
    }
    nbt_move_to_parent(nbt);
}

DimensionType dimension_type_from_json(JSON* json, Arena scratch_arena, Arena* persistent_arena) {
    UNUSED(scratch_arena);
    DimensionType new_type;

    if (json_move_to_cstr(json, "fixed_time") == JSONE_OK) {
        new_type.fixed_time = json_get_int(json);
        json_move_to_parent(json);
    }

    json_easy_get_bool(json, new_type, has_skylight);
    json_easy_get_bool(json, new_type, has_ceiling);
    json_easy_get_bool(json, new_type, ultrawarm);
    json_easy_get_bool(json, new_type, natural);

    json_move_to_cstr(json, "coordinate_scale");
    new_type.coordinate_scale = json_get_float(json);
    json_move_to_parent(json);

    json_easy_get_bool(json, new_type, bed_works);
    json_easy_get_bool(json, new_type, respawn_anchor_works);
    json_easy_get_int(json, new_type, min_y);
    json_easy_get_int(json, new_type, height);
    json_easy_get_int(json, new_type, logical_height);

    json_move_to_cstr(json, "infiniburn");
    string* tmp = json_get_string(json); // Allocated with `scratch`, in json data
    resid_parse(tmp, persistent_arena, &new_type.infiniburn);
    json_move_to_parent(json);

    json_move_to_cstr(json, "effects");
    tmp = json_get_string(json); // Allocated with `scratch`, in json data
    resid_parse(tmp, persistent_arena, &new_type.effects);
    json_move_to_parent(json);

    json_move_to_cstr(json, "ambient_light");
    new_type.ambient_light = json_get_float(json);
    json_move_to_parent(json);
    json_easy_get_bool(json, new_type, piglin_safe);
    json_easy_get_bool(json, new_type, has_raids);
    json_move_to_cstr(json, "monster_spawn_light_level");
    new_type.monster_spawn_light_level =
        int_provider_from_json(json, scratch_arena, persistent_arena);
    json_move_to_parent(json);
    json_easy_get_int(json, new_type, monster_spawn_block_light_limit);

    return new_type;
}

void dimension_type_to_nbt(const DimensionType* obj,
                           Arena scratch_arena,
                           Arena* persistent_arena,
                           NBT* out_nbt) {
    UNUSED(scratch_arena);
    *out_nbt = nbt_create(persistent_arena, 32);

    if (obj->fixed_time >= 0)
        nbt_easy_long(out_nbt, obj, fixed_time);
    nbt_easy_bool(out_nbt, obj, has_skylight);
    nbt_easy_bool(out_nbt, obj, has_ceiling);
    nbt_easy_bool(out_nbt, obj, ultrawarm);
    nbt_easy_bool(out_nbt, obj, natural);
    nbt_easy_double(out_nbt, obj, coordinate_scale);
    nbt_easy_bool(out_nbt, obj, bed_works);
    nbt_easy_bool(out_nbt, obj, respawn_anchor_works);
    nbt_easy_int(out_nbt, obj, min_y);
    nbt_easy_int(out_nbt, obj, height);
    nbt_easy_int(out_nbt, obj, logical_height);
    string tmp_str   = str_view("infiniburn");
    string tmp_value = resid_to_string(&obj->infiniburn, &scratch_arena);
    nbt_put_str(out_nbt, &tmp_str, &tmp_value);

    tmp_str   = str_view("effects");
    tmp_value = resid_to_string(&obj->effects, &scratch_arena);
    nbt_put_str(out_nbt, &tmp_str, &tmp_value);

    nbt_easy_float(out_nbt, obj, ambient_light);
    nbt_easy_bool(out_nbt, obj, piglin_safe);
    nbt_easy_bool(out_nbt, obj, has_raids);
    int_provider_to_nbt(out_nbt, &obj->monster_spawn_light_level, str_view("monster_spawn_light_level"), scratch_arena, persistent_arena);
    nbt_easy_int(out_nbt, obj, monster_spawn_block_light_limit);
}
