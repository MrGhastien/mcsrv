/**
 * @file
 * @author Bastien Morino
 *
 * @brief Functions related to data compression / decompression.
 *
 * Compression and decompression are achieved thanks to ZLib.
 */

#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "containers/bytebuffer.h"
#include "definitions.h"
#include "memory/arena.h"

#include <zlib.h>

// Notchian server compression threshold
#define COMPRESS_THRESHOLD 256

/**
 * Compression context to (de)compress packets.
 *
 * This structure serves as a way to group compression-related information
 * together.
 */
typedef struct {
    z_stream deflate_stream;
    z_stream inflate_stream;
    u64 threshold;
} CompressionContext;

/**
 * Initialize a compression context using the given arena.
 *
 * This functions initializes Zlib inflate and deflate streams, providing the specified arena
 * as a memory allocator.
 *
 * @param[out] ctx The compression context to initialize.
 * @param[in] arena The arena to use as a memory allocator for ZLib.
 * @return @ref TRUE if the context was initialized successfully, @ref FALSE otherwise.
 */
bool compression_init(CompressionContext* ctx, Arena* arena);
/**
 * "Deinitializes" a compression context.
 *
 * This functions closes the ZLib streams initialized when calling compression_init().
 *
 * @param[in] ctx The context to free resources and memory from.
 */
void compression_cleanup(CompressionContext* ctx);

/**
 * Compresses a byte buffer, putting compressed data in another buffer.
 *
 * This function reads bytes from @p in_buffer and leverages ZLib deflate to compress and write
 * data in @p out_buffer.
 *
 * The output buffer must have a sufficient size to store all of the compressed data. A safe
 * way to ensure this is to have the output buffer be the same capacity as the input buffer's
 * size.
 *
 * @param[in] ctx The compression context.
 * @param[out] out_buffer The output buffer, with compressed data written into it.
 * @param[in] in_buffer The input buffer, containing data to compress.
 * @return The length in bytes of the compressed data, or -1 if compression failed.
 */
i64 compression_compress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer);
/**
 * Decompresses a byte buffer, putting uncompressed data in another buffer.
 *
 * This function reads bytes from @p in_buffer and leverages ZLib inflate to decompress and write
 * data in @p out_buffer.
 *
 * The output buffer must have a sufficient size to store all of the uncompressed data.
 *
 * @param[in] ctx The compression context.
 * @param[out] out_buffer The output buffer, with decompressed data written into it.
 * @param[in] in_buffer The input buffer, containing data to decompress.
 * @return The length in bytes of the uncompressed data, or -1 if decompression failed.
 */
i64 compression_decompress(CompressionContext* ctx, ByteBuffer* out_buffer, ByteBuffer* in_buffer);

#endif /* ! COMPRESSION_H */
