#include "iomux.h"

#include "bitwise.h"
#include "containers/bytebuffer.h"
#include "containers/object_pool.h"
#include "memory/arena.h"
#include "str_builder.h"
#include "utils/string.h"

#include <errno.h>
#include <logger.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define MAX_MULTIPLEXERS 1024

void strbuild_append_buf(StringBuilder* builder, const char* buf, u64 size);
void strbuild_insert_buf(StringBuilder* builder, u64 index, const char* buf, u64 size);

static Arena arena;
static ObjectPool multiplexers;

union IOBackend {
    ByteBuffer* buffer;
    FILE* file;
    gzFile gzFile;
    struct {
        StringBuilder builder;
        Arena* arena;
        u64 cursor;
    } string_backend;
};

typedef struct IOMux {
    enum IOType type;
    union IOBackend backend;
    i32 error;
} IOMux_t;

static IOMux iomux_create(enum IOType type, union IOBackend backend) {

    if (!arena.block) {
        arena = arena_create(MAX_MULTIPLEXERS * sizeof(IOMux_t) << 1, BLK_TAG_PLATFORM);
        objpool_init(&multiplexers, &arena, MAX_MULTIPLEXERS, sizeof(IOMux_t));
    }

    i64 index;
    IOMux_t* mux = objpool_add(&multiplexers, &index);
    mux->backend = backend;
    mux->type = type;
    return index;
}

static inline IOMux_t* iomux_get(i64 index) {
    return objpool_get(&multiplexers, index);
}

static i32 retrieve_gz_error(gzFile gzfile) {
    i32 code;
    gzerror(gzfile, &code);
    if (code == Z_OK)
        code = errno;
    return code;
}

IOMux iomux_wrap_buffer(ByteBuffer* buffer) {
    return iomux_create(IO_BUFFER, (union IOBackend){.buffer = buffer});
}
IOMux iomux_wrap_stdfile(FILE* file) {
    return iomux_create(IO_FILE, (union IOBackend){.file = file});
}
IOMux iomux_wrap_gz(gzFile file) {
    return iomux_create(IO_GZFILE, (union IOBackend){.gzFile = file});
}

IOMux iomux_open(const string* path, const char* mode) {
    FILE* file = fopen(path->base, mode);
    if (!file)
        return -1;
    return iomux_create(IO_FILE, (union IOBackend){.file = file});
}
IOMux iomux_gz_open(const string* path, const char* mode) {
    gzFile file = gzopen(path->base, mode);
    if (!file)
        return -1;

    return iomux_create(IO_GZFILE, (union IOBackend){.gzFile = file});
}

IOMux iomux_new_string(Arena* arena) {
    union IOBackend backend = {
        .string_backend =
            {
                             .arena = arena,
                             .builder = strbuild_create(arena),
                             .cursor = 0,
                             },
    };
    return iomux_create(IO_STRING, backend);
}

i32 iomux_write(IOMux multiplexer, const void* data, u64 size) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        res = fwrite(data, size, 1, mux->backend.file);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzwrite(mux->backend.gzFile, data, size);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER:
        bytebuf_write(mux->backend.buffer, data, size);
        break;
    case IO_STRING:
        strbuild_append_buf(&mux->backend.string_backend.builder, data, size);
        break;
    }
    return res;
}

i32 iomux_read(IOMux multiplexer, void* data, u64 size) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        if (!data) {
            if (fseek(mux->backend.file, size, SEEK_CUR) == 0)
                res = fread(NULL, 0, 0, mux->backend.file);
        } else
            res = fread(data, 1, size, mux->backend.file);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_GZFILE:
        if (!data) {
            if (gzseek(mux->backend.gzFile, size, SEEK_CUR) >= 0)
                res = gzread(mux->backend.gzFile, NULL, 0);
        }
        res = gzread(mux->backend.gzFile, data, size);
        if (res < 0)
            mux->error = retrieve_gz_error(mux->backend.gzFile);
        break;
    case IO_BUFFER:
        bytebuf_read(mux->backend.buffer, size, data);
        break;
    case IO_STRING: {
        u64 cursor = mux->backend.string_backend.cursor;
        res = strbuild_get_range(&mux->backend.string_backend.builder, data, cursor, cursor + size);
        if (res < 0)
            mux->error = ESPIPE;
        else
            mux->backend.string_backend.cursor += res;
        break;
    }
    }
    return res;
}

i32 iomux_getc(IOMux multiplexer) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        res = fgetc(mux->backend.file);
        if (res == EOF)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzgetc(mux->backend.gzFile);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER: {
        unsigned char c;
        if (bytebuf_read(mux->backend.buffer, sizeof c, &c) <= 0)
            res = -1;
        else
            res = (i32) c;
        break;
    }
    case IO_STRING:
        return strbuild_get(&mux->backend.string_backend.builder,
                            mux->backend.string_backend.cursor);
    }
    return res == 0;
}
i32 iomux_ungetc(IOMux multiplexer, i32 c) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = c;

    switch (mux->type) {
    case IO_FILE:
        res = ungetc(c, mux->backend.file);
        if (res == EOF)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzungetc(c, mux->backend.gzFile);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER: {
        unsigned char chr = c & 0xff;
        bytebuf_unwrite(mux->backend.buffer, sizeof c);
        bytebuf_write(mux->backend.buffer, &chr, sizeof c);
        break;
    }
    case IO_STRING:
        if (!vect_pop(&mux->backend.string_backend.builder.chars, NULL))
            res = -1;
        strbuild_appendc(&mux->backend.string_backend.builder, c);
        break;
    }
    return res & 0xff;
}

i32 iomux_writef(IOMux multiplexer, const char* format, ...) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;
    va_list args;
    va_start(args, format);

    switch (mux->type) {
    case IO_FILE:
        res = vfprintf(mux->backend.file, format, args);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzvprintf(mux->backend.gzFile, format, args);
        if (res < 0)
            mux->error = retrieve_gz_error(mux->backend.gzFile);
        break;
    case IO_BUFFER: {
        Arena scratch = arena;
        u64 size;
        char* formatted = format_str(&scratch, format, args, &size);
        bytebuf_write(mux->backend.buffer, formatted, size * sizeof *formatted);
    } break;
    case IO_STRING: {
        res = strbuild_appendvf(&mux->backend.string_backend.builder, format, args);
        if (res < 0)
            mux->error = ESPIPE;
        else
            mux->backend.string_backend.cursor += res;
        break;
    }
    }
    va_end(args);
    return res;
}
bool iomux_writec(IOMux multiplexer, i32 chr) {

    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        res = fputc(chr, mux->backend.file);
        if (res == EOF)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzputc(mux->backend.gzFile, chr);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER: {
        unsigned char c = chr & 0xff;
        bytebuf_write(mux->backend.buffer, &c, sizeof c);
        break;
    }
    case IO_STRING:
        strbuild_appendc(&mux->backend.string_backend.builder, chr);
        break;
    }
    return res == 0;
}
i32 iomux_writes(IOMux multiplexer, const char* cstr) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        res = fputs(cstr, mux->backend.file);
        if (res == EOF)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzputs(mux->backend.gzFile, cstr);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER: {
        u64 len = strlen(cstr);
        bytebuf_write(mux->backend.buffer, cstr, len * sizeof *cstr);
        res = len;
        break;
    }
    case IO_STRING:
        strbuild_appends(&mux->backend.string_backend.builder, cstr);
        break;
    }
    return res == 0;
}
i32 iomux_write_str(IOMux multiplexer, const string* str) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = 0;

    switch (mux->type) {
    case IO_FILE:
        res = fputs(cstr(str), mux->backend.file);
        if (res == EOF)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzputs(mux->backend.gzFile, cstr(str));
        if (res < 0)
            mux->error = errno;
        break;
    case IO_BUFFER: {
        bytebuf_write(mux->backend.buffer, str->base, str->length * sizeof *str->base);
        res = str->length;
        break;
    }
    case IO_STRING:
        strbuild_append(&mux->backend.string_backend.builder, str);
        res = str->length;
        break;
    }
    return res == 0;
}

bool iomux_eof(IOMux multiplexer) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return TRUE;

    switch (mux->type) {
    case IO_FILE:
        return feof(mux->backend.file);
    case IO_GZFILE:
        return gzeof(mux->backend.gzFile);
    case IO_BUFFER:
        return bytebuf_size(mux->backend.buffer) == 0;
    case IO_STRING:
        return mux->backend.string_backend.cursor == mux->backend.string_backend.builder.chars.size;
    default:
        abort();
        return TRUE;
    }
}
string iomux_error(IOMux multiplexer, i32* out_code) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return str_view(NULL);

    if (out_code)
        *out_code = mux->error;

    switch (mux->type) {
    case IO_FILE:
        return str_view(strerror(mux->error));
    case IO_GZFILE: {
        i32 code;
        const char* msg = gzerror(mux->backend.gzFile, &code);
        if (code == Z_OK)
            msg = strerror(mux->error);
        return str_view(msg);
    }
    case IO_BUFFER:
        return str_view(NULL);
    case IO_STRING:
        return str_view(strerror(mux->error));
    default:
        abort();
        return str_view(NULL);
    }
}

i32 iomux_seek(IOMux multiplexer, i32 off, i32 method) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    switch (mux->type) {
    case IO_FILE:
        return fseek(mux->backend.file, off, method);
    case IO_GZFILE:
        return gzseek(mux->backend.gzFile, off, method);
    case IO_BUFFER:
        return -1;
    case IO_STRING: {
        StringBuilder* builder = &mux->backend.string_backend.builder;
        switch (method) {
        case SEEK_SET:
            mux->backend.string_backend.cursor = off;
            break;
        case SEEK_CUR:
            mux->backend.string_backend.cursor += off;
            break;
        case SEEK_END:
            mux->backend.string_backend.cursor = strbuild_length(builder) - off;
            break;
        default:
            log_errorf("Unknown seek method %i.", method);
            return -1;
        }
        if (mux->backend.string_backend.cursor > strbuild_length(builder))
            mux->backend.string_backend.cursor = strbuild_length(builder);
        return mux->backend.string_backend.cursor;
    }
    }
}
i32 iomux_tell(IOMux multiplexer) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    switch (mux->type) {
    case IO_FILE:
        return ftell(mux->backend.file);
    case IO_GZFILE:
        return gztell(mux->backend.gzFile);
    case IO_BUFFER:
        return -2;
    case IO_STRING:
        return mux->backend.string_backend.cursor;
    }
}

void iomux_close(IOMux multiplexer) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return;

    switch (mux->type) {
    case IO_FILE:
        fclose(mux->backend.file);
        break;
    case IO_GZFILE:
        gzclose(mux->backend.gzFile);
        break;
    default:
        break;
    }

    objpool_remove(&multiplexers, multiplexer);
}

string iomux_string(IOMux multiplexer, Arena* arena) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux || mux->error != 0)
        return str_view(NULL);

    return strbuild_to_string(&mux->backend.string_backend.builder, arena);
}
