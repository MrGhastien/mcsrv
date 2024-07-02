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

ByteBuffer bytebuf_create(u64 size);
ByteBuffer bytebuf_create_fixed(u64 size, Arena* arena);

void bytebuf_destroy(ByteBuffer* buffer);

void* bytebuf_reserve(ByteBuffer* buffer, u64 size);
void bytebuf_copy(ByteBuffer* dst,  const ByteBuffer* src);
void bytebuf_write(ByteBuffer* buffer, void* data, size_t size);
void bytebuf_write_i64(ByteBuffer* buffer, i64 num);
void bytebuf_write_varint(ByteBuffer* buffer, i32 num);

void bytebuf_prepend(ByteBuffer* buffer, void* data, u64 size);
void bytebuf_prepend_varint(ByteBuffer* buffer, i32 num);

void bytebuf_read(ByteBuffer* buffer, u64 size, void* out_data);

#endif /* ! BYTEBUFFER_H */
