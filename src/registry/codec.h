#ifndef CODEC_H
#define CODEC_H

#include "data/json.h"
#include "data/nbt.h"
#include "world/data/damage_type.h"
#include "world/data/dimension_type.h"
#include "world/data/mob_variants.h"
#include "world/data/painting_variant.h"

#define DECLARE_FROM_JSON(T, name)                                                                 \
    T name##_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena);                 \
    void name##_from_json_generic(                                                                 \
        JSON* json, Arena* scratch_arena, Arena* persistent_arena, void* out_data)

#define DECLARE_TO_NBT(T, name)                                                                    \
    void name##_to_nbt(const T* obj, Arena* scratch_arena, Arena* persistent_arena, NBT* out_nbt); \
    void name##_to_nbt_generic(                                                                    \
        const void* obj, Arena* scratch_arena, Arena* persistent_arena, NBT* out_nbt)

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

static inline string json_easy_get_string(JSON* json, const char* name, Arena* scratch) {
    if (json_move_to_cstr(json, name) == JSONE_NOT_FOUND)
        return (string) {};

    if (json_get_type(json) != JSON_STRING)
        return (string) {};
    string* str = json_get_string(json);
    string copy = str_create_copy(*str, scratch);
    json_move_to_parent(json);
    return copy;
}

static inline u64 json_easy_get_enum(
    JSON* json, const char* name, const char** values, u64 value_count, Arena* scratch) {
    string str = json_easy_get_string(json, name, scratch);

    for (u64 i = 0; i < value_count; i++) {
        if (str_compare_cstr(str, values[i]) == 0) {
            return i;
        }
    }
    return value_count;
}

#define nbt_easy_bool(nbt, obj, name)                                                              \
    {                                                                                              \
        nbt_put_simple(                                                                            \
            nbt, STR_LITERAL(#name), NBT_BYTE, (union NBTSimpleValue) {.byte = ((obj)->name)});    \
    }

#define nbt_easy_int(nbt, obj, name)                                                               \
    nbt_put_simple(                                                                                \
        nbt, STR_LITERAL(#name), NBT_INT, (union NBTSimpleValue) {.integer = ((obj)->name)});

#define nbt_easy_long(nbt, obj, name)                                                              \
    {                                                                                              \
        nbt_put_simple(nbt,                                                                        \
                       STR_LITERAL(#name),                                                         \
                       NBT_LONG,                                                                   \
                       (union NBTSimpleValue) {.long_num = ((obj)->name)});                        \
    }

#define nbt_easy_double(nbt, obj, name)                                                            \
    {                                                                                              \
        nbt_put_simple(nbt,                                                                        \
                       STR_LITERAL(#name),                                                         \
                       NBT_DOUBLE,                                                                 \
                       (union NBTSimpleValue) {.double_num = ((obj)->name)});                      \
    }

#define nbt_easy_float(nbt, obj, name)                                                             \
    {                                                                                              \
        nbt_put_simple(nbt,                                                                        \
                       STR_LITERAL(#name),                                                         \
                       NBT_FLOAT,                                                                  \
                       (union NBTSimpleValue) {.float_num = ((obj)->name)});                       \
    }

#define nbt_easy_str(nbt, obj, name)                                                               \
    {                                                                                              \
        nbt_put_str(nbt, STR_LITERAL(#name), (obj)->name);                                       \
    }

#define nbt_easy_str_2(nbt, name, value)                                                           \
    {                                                                                              \
        nbt_put_str(nbt, STR_LITERAL(#name), STR_LITERAL(#name));                                  \
    }

DECLARE_FROM_JSON(DimensionType, dimension_type);
DECLARE_FROM_JSON(DamageType, damage_type);
DECLARE_FROM_JSON(PaintingVariant, painting_variant);
DECLARE_FROM_JSON(WolfVariant, wolf_variant);

DECLARE_TO_NBT(DimensionType, dimension_type);
DECLARE_TO_NBT(DamageType, damage_type);
DECLARE_TO_NBT(PaintingVariant, painting_variant);
DECLARE_TO_NBT(WolfVariant, wolf_variant);

#endif /* ! CODEC_H */
