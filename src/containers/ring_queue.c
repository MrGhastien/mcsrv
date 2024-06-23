#include "ring_queue.h"
#include "logger.h"
#include "memory/arena.h"
#include "utils/bitwise.h"
#include <string.h>

RingQueue rqueue_create(u32 size, u32 stride, Arena* arena) {
    return (RingQueue){
        .block    = arena_allocate(arena, size * stride),
        .start    = 0,
        .end      = 0,
        .capacity = size,
        .stride   = stride,
        .length   = 0,
    };
}

bool rqueue_enqueue(RingQueue* queue, const void* value) {
    if (queue->length == queue->capacity) {
        log_warn("Attempt to enqueue something to a full ring queue.");
        return FALSE;
    }

    void* dest = offset(queue->block, queue->stride * queue->end);
    memcpy(dest, value, queue->stride);
    queue->length++;
    queue->end = (queue->end + 1) % queue->capacity;

    return TRUE;
}

bool rqueue_dequeue(RingQueue* queue, void* out_value) {
    if (queue->length == 0) {
        log_warn("Attempt to dequeue an empty ring queue.");
        return FALSE;
    }

    if (out_value) {
        void* src = offset(queue->block, queue->stride * queue->start);
        memcpy(out_value, src, queue->stride);
    }
    queue->start = (queue->start + 1) % queue->capacity;
    queue->length--;

    return TRUE;
}

const void* rqueue_peek(const RingQueue* queue) {
    if (queue->length == 0) {
        log_warn("Attempt to peek an empty ring queue.");
        return NULL;
    }

    return offset(queue->block, queue->start * queue->stride);
}
