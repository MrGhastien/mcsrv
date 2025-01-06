#include "ring_queue.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include "memory/mem_tags.h"

#include <string.h>

RingQueue rqueue_create(u32 size, u32 stride, Arena* arena) {
    return (RingQueue){
        .block = arena_allocate(arena, size * stride, ALLOC_TAG_VECTOR),
        .start = 0,
        .end = 0,
        .capacity = size,
        .stride = stride,
        .length = 0,
    };
}

void* rqueue_reserve(RingQueue* queue) {
    u32 length = queue->length;
    u32 end = queue->end;
    if (length == queue->capacity) {
        log_fatal("Attempt to enqueue something to a full ring queue.");
        return NULL;
    }

    void* dest = offset(queue->block, queue->stride * end);
    queue->end = (end + 1) % queue->capacity;
    queue->length = length + 1;
    return dest;
}

bool rqueue_enqueue(RingQueue* queue, const void* value) {
    u32 length = queue->length;
    u32 end = queue->end;
    if (length == queue->capacity) {
        log_fatal("Attempt to enqueue something to a full ring queue.");
        return FALSE;
    }

    void* dest = offset(queue->block, queue->stride * end);
    memcpy(dest, value, queue->stride);

    queue->end = (end + 1) % queue->capacity;

    queue->length = length + 1;

    return TRUE;
}

bool rqueue_dequeue(RingQueue* queue, void* out_value) {
    u32 length = queue->length;
    u32 start = queue->start;
    if (queue->length == 0) {
        log_warn("Attempt to dequeue an empty ring queue.");
        return FALSE;
    }

    if (out_value) {
        void* src = offset(queue->block, queue->stride * start);
        memcpy(out_value, src, queue->stride);
    }
    queue->start = (start + 1) % queue->capacity;
    queue->length = length - 1;

    return TRUE;
}

const void* rqueue_peek(const RingQueue* queue) {
    if (queue->length == 0) {
        log_warn("Attempt to peek an empty ring queue.");
        return NULL;
    }

    return offset(queue->block, queue->start * queue->stride);
}
