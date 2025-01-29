#include "compression.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "utils/math.h"

#include <assert.h>
#include <stdlib.h>
#include <zlib.h>

#define CHUNK 16384

typedef i32 (*zlib_action)(z_streamp stream, int flush);
typedef i32 (*zlib_resetter)(z_streamp stream);

static void* zlib_alloc(void* arena, u32 item_count, u32 size) {
    return arena_allocate(arena, item_count * size, ALLOC_TAG_EXTERNAL);
}

static void zlib_free(void* arena, void* addr) {
    (void) arena;
    (void) addr;
    log_debug("Cleaning up the zlib context.");
    arena_free_ptr(arena, addr);
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
    ctx->threshold = COMPRESS_THRESHOLD;
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

    int flush;

    u8 outBuf[CHUNK];

    u64 region_count = 2;
    BufferRegion regions[2];
    if (bytebuf_get_read_regions(in_buffer, regions, &region_count, 0) == 0)
        return 0;

    stream->avail_out = CHUNK;
    stream->next_out = outBuf;

    /*
      Here we don't copy any data from the input buffer.
      Pointers to areas inside the buffers are given to ZLib to read.
     */
    i32 res = Z_OK;
    u64 region_index = 1;
    stream->avail_in = regions[0].size;
    stream->next_in = regions[0].start;
    do {

        /*
          We have to tell ZLib that it reached the end of the input data !
          */
        flush = region_index >= region_count ? Z_FINISH : Z_NO_FLUSH;

        while (res == Z_OK) {
            res = action(stream, flush);
        }
        switch (res) {
        case Z_BUF_ERROR:
            if (stream->avail_out == 0) {
                bytebuf_write(out_buffer, outBuf, CHUNK - stream->avail_out);
                stream->avail_out = CHUNK;
                stream->next_out = outBuf;
            }
            if (stream->avail_in == 0) {
                if (flush == Z_FINISH) {
                    log_fatal("ZLib has no more output but we told it we have no more input WTF ?");
                    abort();
                }
                stream->avail_in = regions[region_index].size;
                stream->next_in = regions[region_index].start;
                region_index++;
            }
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

i64 compression_decompress_from(
    CompressionContext* ctx, IOMux mux, void* out_buffer, u64 length, u64 output_length) {
    int flush;

    u8 file_buffer[CHUNK];
    u64 total = 0;

    ctx->inflate_stream.avail_out = output_length;
    ctx->inflate_stream.next_out = out_buffer;

    do {

        i32 res = iomux_read(mux, file_buffer, min_u64(CHUNK, length));
        if (res == -1) {
            log_errorf("Error when decompressing file: %s", iomux_error(mux, NULL));
            return -1;
        }
        length -= res;
        ctx->inflate_stream.avail_in = res;
        ctx->inflate_stream.next_in = file_buffer;
        flush = length == 0 || iomux_eof(mux) ? Z_FINISH : Z_NO_FLUSH;

        do {
            res = inflate(&ctx->inflate_stream, flush);
        } while (ctx->inflate_stream.avail_in > 0);

    } while (flush != Z_FINISH);

    bytebuf_register_write(out_buffer, total);
    return output_length - ctx->inflate_stream.avail_out;
}
i64 compression_compress_to(CompressionContext* ctx, IOMux mux, const void* in_buffer, u64 length) {
    u8 file_buffer[CHUNK];
    u64 total = 0;

    ctx->deflate_stream.avail_in = length;
    // WARNING: deleting const !
    ctx->deflate_stream.next_in = (unsigned char*)in_buffer;

    do {
        ctx->deflate_stream.avail_out = CHUNK;
        ctx->deflate_stream.next_out = file_buffer;

        i32 res = deflate(&ctx->inflate_stream, Z_FINISH);
        if (res != Z_OK && res != Z_STREAM_END) {
            log_errorf("ZLib: %s", zError(res));
            deflateReset(&ctx->deflate_stream);
            return -1;
        }

        i32 round_total = CHUNK - ctx->deflate_stream.avail_out;
        if (iomux_write(mux, file_buffer, min_u64(CHUNK, round_total)) != round_total) {
            log_errorf("Error when compressing to file: %s", iomux_error(mux, NULL));
            deflateReset(&ctx->deflate_stream);
            return -1;
        }
        total += round_total;
    } while (ctx->deflate_stream.avail_out == 0);

    assert(ctx->inflate_stream.avail_in == 0);
    return total;
}
i64 compression_compress_buffer_to(CompressionContext* ctx, IOMux mux, ByteBuffer* in_buffer) {
    BufferRegion regions[2];
    u64 region_count = 2;
    u64 region_index = 0;
    bytebuf_get_read_regions(in_buffer, regions, &region_count, 0);
    int flush;

    u8 file_buffer[CHUNK];
    u64 total = 0;

    ctx->deflate_stream.avail_in = regions[region_index].size;
    ctx->deflate_stream.next_in = regions[region_index].start;

    do {

        flush = region_index == region_count ? Z_FINISH : Z_NO_FLUSH;

        do {
            ctx->deflate_stream.avail_out = CHUNK;
            ctx->deflate_stream.next_out = file_buffer;

            i32 res = deflate(&ctx->inflate_stream, flush);
            if (res != Z_STREAM_END) {
                log_errorf("ZLib: %s", zError(res));
                deflateReset(&ctx->deflate_stream);
                return -1;
            }

            i32 round_total = CHUNK - ctx->deflate_stream.avail_out;
            if (iomux_write(mux, file_buffer, min_u64(CHUNK, round_total)) != round_total) {
                log_errorf("Error when compressing to file: %s", iomux_error(mux, NULL));
                deflateReset(&ctx->deflate_stream);
                return -1;
            }
            total += round_total;
        } while (ctx->deflate_stream.avail_out == 0);

        region_index++;
        ctx->deflate_stream.avail_in = regions[region_index].size;
        ctx->deflate_stream.next_in = regions[region_index].start;

    } while (flush != Z_FINISH);

    bytebuf_register_read(in_buffer, bytebuf_size(in_buffer));
    return total;
}
