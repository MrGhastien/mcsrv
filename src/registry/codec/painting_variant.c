#include "world/data/painting_variant.h"
#include "data/json.h"
#include "data/nbt.h"
#include "definitions.h"
#include "registry/codec.h"
#include "registry/codec/internal.h"
#include "utils/string.h"

DEFINE_FROM_JSON(PaintingVariant, painting_variant) {
    PaintingVariant result = {};
    string asset_id        = json_easy_get_string(json, "asset_id", scratch_arena);

    if (!resid_parse(asset_id, persistent_arena, &result.asset_id))
        return result;

    json_easy_get_int(json, result, width);
    json_easy_get_int(json, result, height);

    result.title  = json_easy_get_string(json, "title", scratch_arena);
    result.author = json_easy_get_string(json, "author", scratch_arena);

    return result;
}

static void string_to_text_component_nbt(string text, const string name, NBT* text_nbt) {
    nbt_put(text_nbt, name, NBT_COMPOUND);
    nbt_put_str(text_nbt, STR_LITERAL("text"), text);
    nbt_move_to_parent(text_nbt);
}

DEFINE_TO_NBT(PaintingVariant, painting_variant) {
    UNUSED(persistent_arena);
    *out_nbt = nbt_create(persistent_arena, 32);

    nbt_put_str(out_nbt, STR_LITERAL("asset_id"), resid_to_string(&obj->asset_id, scratch_arena));

    nbt_easy_int(out_nbt, obj, width);
    nbt_easy_int(out_nbt, obj, height);

    string_to_text_component_nbt(obj->title, STR_LITERAL("title"), out_nbt);
    string_to_text_component_nbt(obj->author, STR_LITERAL("author"), out_nbt);
}
