#ifndef CODEC_H
#define CODEC_H

#include "data/json.h"
#include "world/data/dimension_type.h"
#include "data/nbt.h"

DimensionType dimension_type_from_json(JSON* json, Arena scratch_arena, Arena* persistent_arena);
void dimension_type_to_nbt(const DimensionType* obj,
                           Arena scratch_arena,
                           Arena* persistent_arena,
                           NBT* out_nbt);

#endif /* ! CODEC_H */
