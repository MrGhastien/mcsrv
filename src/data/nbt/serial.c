//
// Created by bmorino on 20/12/2024.
//
#include "data/nbt/nbt.h"
#include "utils/bitwise.h"

#include <zlib.h>

static void nbt_write_tag(const NBT* nbt, i64* current_index, bool is_in_list, gzFile fd) {
    NBTTag* tag = vector_ref(&nbt->tags, *current_index);
    if (!is_in_list) {
        i8 type_num = tag->type & 0xff;
        gzwrite(fd, &type_num, sizeof type_num);
        const i16 big_endian_name = big_endian16(tag->name.length & 0xffff);
        gzwrite(fd, &big_endian_name, sizeof big_endian_name);
        gzwrite(fd, tag->name.base, tag->name.length);
    }
    switch (tag->type) {
    case NBT_BYTE:
        gzwrite(fd, &tag->data.simple.byte, sizeof tag->data.simple.byte);
        break;
    case NBT_SHORT: {
        const i16 big_endian = big_endian16(tag->data.simple.short_num);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_FLOAT:
    case NBT_INT: {
        const i32 big_endian = big_endian32(tag->data.simple.integer);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_DOUBLE: {
    case NBT_LONG: {
        const i64 big_endian = big_endian64(tag->data.simple.long_num);
        gzwrite(fd, &big_endian, sizeof big_endian);
        break;
    }
    case NBT_STRING: {
        const i32 big_endian = big_endian32(tag->data.str.length & 0xffff);
        gzwrite(fd, &big_endian, sizeof big_endian);
        gzwrite(fd, tag->data.str.base, tag->data.str.length);
        break;
    }
    case NBT_COMPOUND: {
        for (i32 i = 0; i < tag->data.array_size; i++) {
            (*current_index)++;
            nbt_write_tag(nbt, current_index, FALSE, fd);
        }
        const i8 end_tag = 0;
        gzwrite(fd, &end_tag, sizeof end_tag);
        break;
    }
        case NBT_LIST: {
    }
    default:
        break;
    }
    }

    void nbt_write(const NBT* nbt, const string* path) {
    }

    NBT nbt_parse(Arena * arena, i64 max_token_count, const string* path) {
        return TODO_IMPLEMENT_ME;
    }
