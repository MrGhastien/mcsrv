//
// Created by bmorino on 06/01/2025.
//

#include "mem_tags.h"

#include "definitions.h"
#include "utils/str_builder.h"

static const char* TAG_NAMES[] = {
    [0] = "Unknown",
    "Vector",
    "Object pool",
    "String",
    "Dictionary",
    [16] = "",
    "NBT",
    "JSON",
    "TOML",
    [32] = "",
    "Network",
    "Event",
    "Registry",
    "Platform",
};

string get_tag_names(i32 tags, Arena* arena) {
    i32 container = tags & 0xf;
    i32 format = (tags >> 4) & 0xf;
    i32 module = (tags >> 8) & 0xf;

    const char* container_name = TAG_NAMES[container];
    const char* format_name = TAG_NAMES[format + 16];
    const char* module_name = TAG_NAMES[format + 32];

    StringBuilder builder = strbuild_create(arena);

    strbuild_appends(&builder, container_name);
    if (format != 0) {
        strbuild_appends(&builder, ", ");
        strbuild_appends(&builder, format_name);
    }
    if (module != 0) {
        strbuild_appends(&builder, ", ");
        strbuild_appends(&builder, module_name);
    }

    return strbuild_to_string(&builder, arena);
}