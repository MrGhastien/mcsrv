#ifndef INTERNAL_H
#define INTERNAL_H

#include <string.h>

#define DEFINE_FROM_JSON(T, name)                                                                  \
    void name##_from_json_generic(                                                                 \
        JSON* json, Arena* scratch_arena, Arena* persistent_arena, void* out_data) {                \
        T value = name##_from_json(json, scratch_arena, persistent_arena);                         \
        memcpy(out_data, &value, sizeof(T));                                                        \
    }                                                                                              \
    T name##_from_json(JSON* json, Arena* scratch_arena, Arena* persistent_arena)

#define DEFINE_TO_NBT(T, name)                                                                     \
    void name##_to_nbt_generic(                                                                    \
        const void* obj, Arena* scratch_arena, Arena* persistent_arena, NBT* out_nbt) {             \
        name##_to_nbt((const T*) obj, scratch_arena, persistent_arena, out_nbt);                   \
    }                                                                                              \
    void name##_to_nbt(const T* obj, Arena* scratch_arena, Arena* persistent_arena, NBT* out_nbt)

#endif /* ! INTERNAL_H */
