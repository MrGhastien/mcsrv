#include "iomux.h"

#include "bitwise.h"
#include "containers/bytebuffer.h"
#include "event/event.h"
#include "memory/_memory_internal.h"
#include "memory/allocators/pool.h"
#include "memory/memory.h"
#include "network/compression.h"
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
#define INFLATE_BUFFER_SIZE 512

void strbuild_append_buf(StringBuilder* builder, const char* buf, u64 size);
void strbuild_insert_buf(StringBuilder* builder, u64 index, const char* buf, u64 size);

typedef struct IOMux IOMux_t;

union IOBackend {
    ByteBuffer* buffer;
    FILE* file;
    gzFile gzFile;
    struct string_backend {
        StringBuilder builder;
        Arena* arena;
        u64 cursor;
    } string_backend;
    struct zlib_backend {
        IOMux source;
        CompressionContext ctx;
        u8 in_block[INFLATE_BUFFER_SIZE];
        i64 max_in;
        ByteBuffer inflate_buffer;
    } zlib;
};

struct IOMux {
    enum IOType type;
    union IOBackend backend;
    i32 error;
    u64 total_read;
};

static PoolAllocator multiplexers = {.mem = INVALID_CHAIN};

static bool iomux_system_cleanup(u32 event, void* user_data, EventInfo info) {
    (void) event;
    (void) user_data;
    (void) info;
    pool_destroy(&multiplexers);
    return TRUE;
}

static IOMux iomux_create(enum IOType type, union IOBackend backend) {

    if (multiplexers.mem == INVALID_CHAIN) {
        pool_init(
            &multiplexers, MAX_MULTIPLEXERS, sizeof(IOMux_t), BLK_TAG_PLATFORM, INVALID_CHAIN);
        event_register_listener(BEVENT_STOP, &iomux_system_cleanup, NULL);
    }

    i64 index;
    IOMux_t* mux    = pool_alloc(&multiplexers, &index);
    mux->total_read = 0;
    mux->backend    = backend;
    mux->type       = type;
    return index;
}

static inline IOMux_t* iomux_get(i64 index) {
    return pool_get(&multiplexers, index);
}

static i32 retrieve_gz_error(gzFile gzfile) {
    i32 code;
    gzerror(gzfile, &code);
    if (code == Z_OK)
        code = errno;
    return code;
}

IOMux iomux_wrap_buffer(ByteBuffer* buffer) {
    return iomux_create(IO_BUFFER, (union IOBackend) {.buffer = buffer});
}
IOMux iomux_wrap_stdfile(FILE* file) {
    return iomux_create(IO_FILE, (union IOBackend) {.file = file});
}
IOMux iomux_wrap_gz(gzFile file) {
    return iomux_create(IO_GZFILE, (union IOBackend) {.gzFile = file});
}
IOMux iomux_wrap_zlib(IOMux compressed_stream, i64 size, Arena* arena) {
    IOMux_t* mux = iomux_get(compressed_stream);
    if (!mux || mux->type == IO_STRING)
        return -1;

    // WARNING: Do not use the `in_block` from this backend: The address will become invalid after
    // we exit the function ! During testing, this causes SIGSEGV later when reading data from a
    // region file and returning from the function (the stack is rewritten, including the return
    // address !)

    union IOBackend backend = {
        .zlib = {.source = compressed_stream, .max_in = size}
    };
    IOMux new_mux        = iomux_create(IO_ZLIB, backend);
    IOMux_t* new_mux_obj = iomux_get(new_mux);
    new_mux_obj->backend.zlib.inflate_buffer =
        bytebuf_wrap(INFLATE_BUFFER_SIZE, new_mux_obj->backend.zlib.in_block);
    compression_init(&new_mux_obj->backend.zlib.ctx, arena);
    return new_mux;
}

IOMux iomux_open(const string* path, const char* mode) {
    FILE* file = fopen(path->base, mode);
    if (!file)
        return -1;
    return iomux_create(IO_FILE, (union IOBackend) {.file = file});
}
IOMux iomux_gz_open(const string* path, const char* mode) {
    gzFile file = gzopen(path->base, mode);
    if (!file)
        return -1;

    return iomux_create(IO_GZFILE, (union IOBackend) {.gzFile = file});
}

IOMux iomux_new_string(Arena* arena) {
    union IOBackend backend = {
        .string_backend =
            {
                             .arena   = arena,
                             .builder = strbuild_create(arena),
                             .cursor  = 0,
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
    case IO_ZLIB:
        return compression_compress_to(
            &mux->backend.zlib.ctx, mux->backend.zlib.source, data, size);
    default:
        abort();
        break;
    }
    return res;
}

i32 iomux_read(IOMux multiplexer, void* data, u64 size) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    if (mux->error != 0)
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
        res = bytebuf_read(mux->backend.buffer, size, data);
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
    case IO_ZLIB:
        res = compression_decompress_from(&mux->backend.zlib.ctx,
                                          mux->backend.zlib.source,
                                          &mux->backend.zlib.inflate_buffer,
                                          data,
                                          &mux->backend.zlib.max_in,
                                          size);
        if (res < 0)
            mux->error = res;
        break;
    default:
        abort();
        break;
    }
    if (mux->error == 0)
        mux->total_read += res;
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
    case IO_ZLIB: {
        unsigned char c;
        res = iomux_read(multiplexer, &c, 1);
        break;
    }
    default:
        abort();
        break;
    }
    mux->total_read += res == 0;
    return res;
}
i32 iomux_ungetc(IOMux multiplexer, i32 c) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return -1;

    i32 res = c & 0xff;

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
        bytebuf_unwrite(mux->backend.buffer, 1);
        bytebuf_write(mux->backend.buffer, &chr, 1);
        bytebuf_unwrite(mux->backend.buffer, 1);
        break;
    }
    case IO_STRING:
        if (!vect_pop(&mux->backend.string_backend.builder.chars, NULL))
            res = -1;
        strbuild_appendc(&mux->backend.string_backend.builder, c);
        break;
    default:
        abort();
        return -1;
    }
    return res;
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
        Arena scratch = arena_create(8192, BLK_TAG_PLATFORM, INVALID_CHAIN);
        u64 size;
        char* formatted = format_cstr(&scratch, format, args, &size);
        bytebuf_write(mux->backend.buffer, formatted, size * sizeof *formatted);
        arena_destroy(&scratch);
        res = size;
    } break;
    case IO_STRING: {
        res = strbuild_appendvf(&mux->backend.string_backend.builder, format, args);
        if (res < 0)
            mux->error = ESPIPE;
        else
            mux->backend.string_backend.cursor += res;
        break;
    }
    case IO_ZLIB: {
        Arena scratch = arena_create(8192, BLK_TAG_PLATFORM, INVALID_CHAIN);
        u64 size;
        char* formatted = format_cstr(&scratch, format, args, &size);
        compression_compress_to(&mux->backend.zlib.ctx, mux->backend.zlib.source, formatted, size);
        arena_destroy(&scratch);
        res = size;
        break;
    }
    default:
        abort();
        break;
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
    case IO_ZLIB:
        log_fatal(" TODO: iomux_writec for zlib");
        res = -1;
        break;
    default:
        abort();
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
    case IO_ZLIB:
        log_fatal(" TODO: iomux_writes for zlib");
        res = -1;
        break;
    default:
        abort();
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
    case IO_ZLIB:
        log_fatal(" TODO: iomux_write_str for zlib");
        res = -1;
        break;
    default:
        abort();
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
    case IO_ZLIB:
        return mux->backend.zlib.max_in == 0 && mux->backend.zlib.inflate_buffer.size == 0;
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
    case IO_ZLIB:
        return iomux_error(mux->backend.zlib.source, out_code);
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
    case IO_ZLIB:
        log_error("Seeking mechanism not implemented for the IOMux ZLib backend");
        return -1;
    default:
        abort();
        break;
    }
    return -1;
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
    case IO_ZLIB:
        log_error("Seeking mechanism not implemented for the IOMux ZLib backend");
        return -1;
    default:
        abort();
        break;
    }
    return -1;
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
    case IO_ZLIB:
        compression_cleanup(&mux->backend.zlib.ctx);
        break;
    default:
        break;
    }

    pool_free_idx(&multiplexers, multiplexer);
}

string iomux_string(IOMux multiplexer, Arena* arena) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux || mux->error != 0)

        return str_view(NULL);

    return strbuild_to_string(&mux->backend.string_backend.builder, arena);
}
