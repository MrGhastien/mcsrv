#include "compression.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include "utils/math.h"
#include <zlib.h>

typedef i32 (*zlib_action)(z_streamp stream);
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
    if(code != Z_OK) {
        log_errorf("ZLib: %s.", ctx->deflate_stream.msg);
        log_error("Failed to initialize the compression context.");
        return FALSE;
    }
    code = inflateInit(&ctx->inflate_stream);
    if(code != Z_OK) {
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

static i32 zlib_deflate(z_streamp stream) {
    return deflate(stream, Z_NO_FLUSH);
}

static i32 zlib_inflate(z_streamp stream) {
    return inflate(stream, Z_NO_FLUSH);
}

static i32 zlib_reset_deflate(z_streamp stream) {
    return deflateReset(stream);
}

static i32 zlib_reset_inflate(z_streamp stream) {
    return inflateReset(stream);
}

static i64 zlib_execute(CompressionContext* ctx,
                        ByteBuffer* out_buffer,
                        ByteBuffer* in_buffer,
                        zlib_action action,
                        zlib_resetter resetter) {
    u64 to_compress = min_u64(in_buffer->capacity - in_buffer->read_head, in_buffer->size);
    u64 to_write = min_u64(out_buffer->capacity - out_buffer->write_head,
                           out_buffer->capacity - out_buffer->size);
    bool in_split = to_compress < in_buffer->size;

    ctx->deflate_stream.next_in = offset(in_buffer->buf, in_buffer->read_head);
    ctx->deflate_stream.avail_in = to_compress;
    ctx->deflate_stream.next_out = offset(out_buffer->buf, out_buffer->write_head);
    ctx->deflate_stream.avail_out = to_write;

    i32 res = Z_OK;
    bool can_compress = TRUE;
    while (can_compress) {

        while (res == Z_OK) {
            res = action(&ctx->deflate_stream);
        }
        switch (res) {
        case Z_STREAM_END:
            if (in_split) {
                ctx->deflate_stream.avail_in = in_buffer->size - to_compress;
                ctx->deflate_stream.next_in = in_buffer->buf;
                in_split = FALSE;
            } else
                can_compress = FALSE;
            break;

        case Z_BUF_ERROR:
            if (ctx->deflate_stream.total_out >= out_buffer->capacity - out_buffer->size) {
                log_error("Compression output buffer is too small.");
                return -1;
            }
            ctx->deflate_stream.avail_out = out_buffer->size - to_write;
            ctx->deflate_stream.next_out = out_buffer->buf;
            break;

        default:
            log_error("Error when compressing packet.");
            return -1;
        }
    }
    u64 written = ctx->deflate_stream.total_out;
    out_buffer->size += written;
    out_buffer->write_head += written;
    if (resetter(&ctx->deflate_stream) != Z_OK) {
        log_error("Could not end the compression stream.");
        return -1;
    }
    return written;
}

i64 compression_compress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer) {
    return zlib_execute(ctx, out_buffer, in_buffer, &zlib_deflate, &zlib_reset_deflate);
}

i64 compression_decompress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer) {
    return zlib_execute(ctx, out_buffer, in_buffer, &zlib_inflate, &zlib_reset_inflate);
}
