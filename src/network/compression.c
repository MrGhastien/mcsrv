#include "compression.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"

#include <zlib.h>

#define CHUNK 16384

typedef i32 (*zlib_action)(z_streamp stream, int flush);
typedef i32 (*zlib_resetter)(z_streamp stream);

static void* zlib_alloc(void* arena, u32 item_count, u32 size) {
    return arena_allocate(arena, item_count * size);
}

static void zlib_free(void* arena, void* addr) {
    (void) arena;
    (void) addr;
    log_debug("Cleaning up the zlib context.");
}

bool compression_init(CompressionContext* ctx, Arena* arena) {

    ctx->deflate_stream.zalloc = &zlib_alloc;
    ctx->deflate_stream.zfree = &zlib_free;
    ctx->deflate_stream.opaque = arena;
    ctx->inflate_stream.zalloc = &zlib_alloc;
    ctx->inflate_stream.zfree = &zlib_free;
    ctx->inflate_stream.opaque = arena;

    i32 code = deflateInit(&ctx->deflate_stream, Z_DEFAULT_COMPRESSION);
    if (code != Z_OK) {
        log_errorf("ZLib: %s.", ctx->deflate_stream.msg);
        log_error("Failed to initialize the compression context.");
        return FALSE;
    }
    code = inflateInit(&ctx->inflate_stream);
    if (code != Z_OK) {
        log_errorf("ZLib: %s.", ctx->inflate_stream.msg);
        log_error("Failed to initialize the compression context.");
        return FALSE;
    }
    return TRUE;
}

void compression_cleanup(CompressionContext* ctx) {
    deflateEnd(&ctx->deflate_stream);
    inflateEnd(&ctx->inflate_stream);
}

static i32 zlib_deflate(z_streamp stream, int flush) {
    return deflate(stream, flush);
}

static i32 zlib_inflate(z_streamp stream, int flush) {
    return inflate(stream, flush);
}

static i32 zlib_reset_deflate(z_streamp stream) {
    return deflateReset(stream);
}

static i32 zlib_reset_inflate(z_streamp stream) {
    return inflateReset(stream);
}

static i64 zlib_execute(z_streamp stream,
                        ByteBuffer* out_buffer,
                        ByteBuffer* in_buffer,
                        zlib_action action,
                        zlib_resetter resetter) {

    void* tmp;
    int flush;

    u8 outBuf[CHUNK];

    /*
      Here we don't copy any data from the input buffer.
      Pointers to areas inside the buffers are given to ZLib to read.
     */
    stream->avail_out = CHUNK;
    stream->next_out = outBuf;

    i32 res = Z_OK;
    do {
        stream->avail_in = bytebuf_contiguous_read(in_buffer, &tmp);
        stream->next_in = tmp;
        /*
          We have to tell ZLib that it reached the end of the input data !
          */
        flush = in_buffer->size == 0 ? Z_FINISH : Z_NO_FLUSH;

        while (res == Z_OK) {
            res = action(stream, flush);
        }
        switch (res) {
        case Z_BUF_ERROR:
            bytebuf_write(out_buffer, outBuf, CHUNK - stream->avail_out);
            stream->avail_out = CHUNK;
            stream->next_out = outBuf;
            break;

        case Z_STREAM_ERROR:
            log_error("Error when (de)compressing packet.");
            return -1;
        }
    } while (flush != Z_FINISH);

    bytebuf_write(out_buffer, outBuf, CHUNK - stream->avail_out);
    if (resetter(stream) != Z_OK) {
        log_error("Could not end the compression stream.");
        return -1;
    }
    return stream->total_out;
}

i64 compression_compress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer) {
    return zlib_execute(
        &ctx->deflate_stream, out_buffer, in_buffer, &zlib_deflate, &zlib_reset_deflate);
}

i64 compression_decompress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer) {
    return zlib_execute(
        &ctx->inflate_stream, out_buffer, in_buffer, &zlib_inflate, &zlib_reset_inflate);
}
