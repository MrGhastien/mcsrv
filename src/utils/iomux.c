#include "iomux.h"

#include "containers/bytebuffer.h"
#include "containers/object_pool.h"
#include "memory/arena.h"
#include "utils/string.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define MAX_MULTIPLEXERS 1024

static Arena arena;
static ObjectPool multiplexers;

union IOBackend {
    ByteBuffer* buffer;
    FILE* file;
    gzFile gzFile;
};

typedef struct IOMux {
    enum IOType type;
    union IOBackend backend;
    i32 error;
} IOMux_t;

static IOMux iomux_create(enum IOType type, union IOBackend backend) {

    if (!arena.block) {
        arena = arena_create(MAX_MULTIPLEXERS * sizeof(IOMux_t));
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
    if(code == Z_OK)
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
        abort();
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
        res = fread(data, size, 1, mux->backend.file);
        if (res < 0)
            mux->error = errno;
        break;
    case IO_GZFILE:
        res = gzread(mux->backend.gzFile, data, size);
        if (res < 0)
            mux->error = retrieve_gz_error(mux->backend.gzFile);
        break;
    case IO_BUFFER:
        bytebuf_read(mux->backend.buffer, size, data);
        break;
    case IO_STRING:
        abort();
        break;
    }
    return res;
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
    default:
        abort();
        return TRUE;
    }
}
string iomux_error(IOMux multiplexer, i32* out_code) {
    IOMux_t* mux = iomux_get(multiplexer);
    if (!mux)
        return str_create_const(NULL);

    if (out_code)
        *out_code = mux->error;

    switch (mux->type) {
    case IO_FILE:
        return str_create_const(strerror(mux->error));
    case IO_GZFILE: {
        i32 code;
        const char* msg = gzerror(mux->backend.gzFile, &code);
        if (code == Z_OK)
            msg = strerror(mux->error);
        return str_create_const(msg);
    }
    case IO_BUFFER:
        return str_create_const(NULL);
    default:
        abort();
        return str_create_const(NULL);
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
}
