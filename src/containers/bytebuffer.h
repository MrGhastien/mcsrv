#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

#include "definitions.h"
#include "memory/arena.h"

/**
   A byte buffer. Acts as a queue for bytes.

   Used to store bytes in order, then send them through a socket.
 */
typedef struct byte_buffer {
    void* buf;
    i64 read_head;
    i64 write_head;
    u64 size;
    u64 capacity;
} ByteBuffer;

/**
 * Create a dynamic byte buffer, starting with the given size.
 *
 * @param size The size of the buffer.
 *
 * @return The newly created byte buffer
 */
ByteBuffer bytebuf_create(u64 size);

/**
 * Create a static byte buffer, starting with the given size,
 * and allocated with the given arena.
 *
 * @param size The size of the buffer.
 * @param arena The arena used to allocate the buffer.
 * @return The newly created byte buffer
 */
ByteBuffer bytebuf_create_fixed(u64 size, Arena* arena);

void bytebuf_destroy(ByteBuffer* buffer);

void* bytebuf_reserve(ByteBuffer* buffer, u64 size);
void bytebuf_copy(ByteBuffer* dst,  const ByteBuffer* src);
void bytebuf_write_buffer(ByteBuffer* dst, const ByteBuffer* src);
void bytebuf_write(ByteBuffer* buffer, void* data, size_t size);
void bytebuf_write_i64(ByteBuffer* buffer, i64 num);
void bytebuf_write_varint(ByteBuffer* buffer, i32 num);

void bytebuf_prepend(ByteBuffer* buffer, void* data, u64 size);
void bytebuf_prepend_varint(ByteBuffer* buffer, i32 num);

i64 bytebuf_read(ByteBuffer* buffer, u64 size, void* out_data);
i64 bytebuf_peek(ByteBuffer* buffer, u64 size, void* out_data);

/**
 * Get the size of the next contiguous readable region of the buffer,
 * after the read head.
 *
 * @param buffer The byte buffer
 * @param out_region Where the region's pointer is copied at.
 * @return The size of the readable region
 */
u64 bytebuf_contiguous_read(ByteBuffer* buffer, void** out_region);

/**
 * Get the size of the next contiguous writable region of the buffer,
 * after the write head.
 *
 * @param buffer The byte buffer
 * @param out_region Where the region's pointer is copied at.
 * @return The size of the writable region
 */
u64 bytebuf_contiguous_write(ByteBuffer* buffer, void** out_region);

#endif /* ! BYTEBUFFER_H */
