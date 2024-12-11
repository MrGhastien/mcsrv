/**
 * @file bytebuffer.h
 * @author Bastien Morino
 *
 * @brief Functions related to byte buffers.
 *
 * A byte buffer stores bytes in a First In, First Out fashion (like a queue/FIFO). Byte buffers can
 *be **fixed** of **dynamic**.
 * ## Fixed byte buffers
 * Fixed byte buffers work as a ring queue; writing data after the end of a buffer would
 * instead write it at the beginning.
 *
 * Fixed byte buffers can not be resized or destroyed ,as they use arenas to allocate their
 * underlying memory.
 *
 * ## Dynamic byte buffers
 * A dynamic byte buffer is similar to a dynamic queue, only storing bytes.
 * They can be resized or destroyed.
 *
 * ## Operations on buffers
 * Byte buffers support several operations :
 * - @link bytebuf_write write@endlink operations, which write data at the end of the buffer,
 * - @link bytebuf_read read@endlink operations, which query data at the beginning of the buffer,
 * - @link bytebuf_prepend prepend@endlink operations, which write data at the beginning of the
 *uffer.
 *
 * @note Byte buffer-related functions which have one or more pointers to byte buffers
 * as parameters do **NOT** accept `NULL` pointers, unless explicitly specified.
 */
#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

#include "definitions.h"
#include "memory/arena.h"
#include "utils/string.h"

typedef struct byte_buffer {
    void* buf;      /**< @private Underlying memory used to store bytes. */
    i64 read_head;  /**< @private The index at which the next read operation will start. */
    i64 write_head; /**< @private The index at which the next write operation will start. */
    u64 size;       /**< @private The number of bytes inside the buffer. */
    u64 capacity;   /**< @private The maximum capacity of the buffer. */
} ByteBuffer;

typedef struct BufferRegion {
    void* start;
    u64 size;
} BufferRegion;

/**
 * Creates a dynamic byte buffer, starting with the given size.
 *
 * @param[in] size The size of the buffer.
 *
 * @return The newly created byte buffer
 */
ByteBuffer bytebuf_create(u64 size);

/**
 * Create a fixed byte buffer, starting with the given size,
 * and allocated with the given arena.
 *
 * @param size The size of the buffer.
 * @param arena The arena used to allocate the buffer.
 * @return The newly created byte buffer
 */
ByteBuffer bytebuf_create_fixed(u64 size, Arena* arena);

/**
 * Frees memory associated with a byte buffer.
 *
 * @param buffer The byte buffer to destroy.
 *
 * @warning Calling this function with a fixed byte buffer will cause the program to abort.
 */
void bytebuf_destroy(ByteBuffer* buffer);

/**
 * Reserves a contiguous memory region inside the specified buffer to write to.
 *
 * @param buffer The byte buffer to reserve memory into.
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory region, or NULL if the byte buffer is fixed.
 */
void* bytebuf_reserve(ByteBuffer* buffer, u64 size);

/**
 * Replaces the contents of a byte buffer with the contents of another.
 *
 * @param dst A pointer to the destination byte buffer whose contents are replaced.
 * @param src A pointer to the source byte buffer.
 */
void bytebuf_copy(ByteBuffer* dst, const ByteBuffer* src);
/**
 * Writes the contents of a byte buffer into another.
 *
 * @param dst The destination byte buffer where data is written.
 * @param src The source byte buffer containing the data to write.
 */
void bytebuf_write_buffer(ByteBuffer* dst, const ByteBuffer* src);
/**
 * Writes arbitrary data into a byte buffer.
 *
 * This functions assumes the given memory region is at least as large as the specified size.
 *
 * @param buffer The byte buffer to write data into.
 * @param data A pointer to the data to write.
 * @param size The amount of bytes to write.
 */
void bytebuf_write(ByteBuffer* buffer, const void* data, size_t size);
/**
 * Writes a 8 bytes (64 bits) signed number into a byte buffer.
 *
 * @param buffer The buffer to write into.
 * @param num The number to write into the buffer.
 */
void bytebuf_write_i64(ByteBuffer* buffer, i64 num);
/**
 * Writes a 23 bit signed integer at the end of a byte buffer, encoded as a Minecraft VarInt.
 *
 * @param buffer The buffer to write into.
 * @param num The number to encode and write into the buffer.
 */
void bytebuf_write_varint(ByteBuffer* buffer, i32 num);
/**
 * Writes arbitrary data at the beginning of a byte buffer.
 *
 * This functions assumes the given memory region is at least as large as the specified size.
 *
 * @param buffer The buffer to prepend data to.
 * @param[in] data A pointer to the data to write.
 * @param size The number of bytes to write.
 */
void bytebuf_prepend(ByteBuffer* buffer, const void* data, u64 size);
/**
 * Writes a 23 bit signed integer at the beginning of a byte buffer, encoded as a Minecraft VarInt.
 *
 * @param buffer The buffer to write data into.
 * @param num The number to encode and write into the buffer.
 */
void bytebuf_prepend_varint(ByteBuffer* buffer, i32 num);

/**
 * Reads arbitrary data from a byte buffer.
 *
 * This functions assumes the memory region pointed to by @p out_data is at least as large as the specified size.
 *
 * @param buffer The buffer to read from.
 * @param size The number of bytes to read.
 * @param[out] out_data A pointer to the memory where data is written.
 * @return The number of bytes read.
   */
i64 bytebuf_read(ByteBuffer* buffer, u64 size, void* out_data);
/**
 * Reads and decodes a MC VarInt from a byte buffer.
 *
 * If the VarInt is invalid or there are not enough bytes inside the buffer,
 * the read-head of the buffer is not incremented.
 * This means attempting a new read will start at the same position.
 *
 * @param buffer The buffer to read from.
 * @param[out] out A pointer to a 32-bit signed integer that will contain the decoded VarInt.
 * @return The number of bytes read and decoded, or :
 * - `0` if there is not enough bytes inside the buffer to read the whole VarInt;
 * - `-1` if the VarInt is invalid.
 */
i64 bytebuf_read_varint(ByteBuffer* buffer, i32* out);
/**
 * Reads a MC string from a byte buffer.
 *
 * Minecraft strings are made of a UTF-8 string prefixed by its length encoded as a VarInt.
 *
 * @param buffer The buffer to read from.
 * @param arena The arena used to allocate the resulting string.
 * @param[out] out_str A pointer to a string structure that will contain the resulting string.
 * @return The number of bytes read.
 */
i64 bytebuf_read_mcstring(ByteBuffer* buffer, Arena* arena, string* out_str);

/**
 * Reads arbitrary data from a byte buffer, without registering the read.
 *
 * Chained calls of this function will start reading from the same point in the buffer.
 * @param buffer The buffer to read from.
 * @param size The number of bytes to read.
 * @param[out] out_data A pointer to the memory where data is written.
 * @return The number of bytes read.
 */
i64 bytebuf_peek(const ByteBuffer* buffer, u64 size, void* out_data);

u64 bytebuf_get_read_regions(const ByteBuffer* buffer, BufferRegion* out_regions, u64* out_count, i64 start_offset);
u64 bytebuf_get_write_regions(const ByteBuffer* buffer, BufferRegion* out_regions, u64* out_count, i64 start_offset);

/**
 * Get the size of the next contiguous readable region of the buffer,
 * after the read head.
 *
 * @param[in,out] buffer The byte buffer
 * @param[out] out_region Where the region's pointer is copied at.
 * @return The size of the readable region
 */
//u64 bytebuf_contiguous_read(ByteBuffer* buffer, void** out_region);

/**
 * Get the size of the next contiguous writable region of the buffer,
 * after the write head.
 *
 * @param[in,out] buffer The byte buffer
 * @param[in] out_region Where the region's pointer is copied at.
 * @return The size of the writable region
 */
//u64 bytebuf_contiguous_write(ByteBuffer* buffer, void** out_region);

/**
 * Removes written data from the specified byte buffer.
 * @param[in,out] buffer The byte buffer to remove data from.
 * @param[in] size The number of bytes to remove.
 */
void bytebuf_unwrite(ByteBuffer* buffer, u64 size);

/**
 * Makes previously read data from the specified byte buffer readable again.
 *
 * At most `buffer->size` bytes can be made readable.
 *
 * @note If one or more write operations are done after the last read operation,
 * an "unread" operation cannot guarantee to restore the same data as the last read data.
 *
 * @param[in] buffer The byte buffer to restore data into.
 * @param[in] size The number of bytes to make readable.
 */
void bytebuf_unread(ByteBuffer* buffer, u64 size);

u64 bytebuf_register_read(ByteBuffer* buffer, u64 size);

u64 bytebuf_register_write(ByteBuffer* buffer, u64 size);

u64 bytebuf_size(const ByteBuffer* buffer);
u64 bytebuf_available(const ByteBuffer* buffer);
u64 bytebuf_cap(const ByteBuffer* buffer);
i64 bytebuf_current_pos(const ByteBuffer* buffer);



#endif /* ! BYTEBUFFER_H */
