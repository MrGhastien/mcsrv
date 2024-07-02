#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "containers/bytebuffer.h"
#include "definitions.h"
#include "memory/arena.h"

#include <zlib.h>

typedef struct {
    z_stream deflate_stream;
    z_stream inflate_stream;
    u64 threadhold;
} CompressionContext;

bool compression_init(CompressionContext* ctx, Arena* arena);
void compression_cleanup(CompressionContext* ctx);

i64 compression_compress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer);
i64 compression_decompress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer);

#endif /* ! COMPRESSION_H */
