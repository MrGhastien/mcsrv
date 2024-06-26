#ifndef RING_QUEUE_H
#define RING_QUEUE_H

#include "definitions.h"
#include "memory/arena.h"

typedef struct ring_queue {
    void* block;
    u32 start;
    u32 end;
    u32 capacity;
    u32 stride;
    u32 length;
} RingQueue;

RingQueue rqueue_create(u32 size, u32 stride, Arena* arena);

void* rqueue_reserve(RingQueue* queue);
bool rqueue_enqueue(RingQueue* queue, const void* value);
bool rqueue_dequeue(RingQueue* queue, void* out_value);
const void* rqueue_peek(const RingQueue* queue);

#endif /* ! CIRCULAR_QUEUE_H */
