#include "mn/queue_spsc.h"

#include "aws/common/atomics.h"

#include "mn/allocator.h"
#include "mn/atomic.h"
#include "mn/error.h"

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_setup(mn_queue_spsc_t *q, uint64_t capacity)
{
    MN_ASSERT(q);
    MN_ASSERT(capacity > 0);

    if (capacity & (capacity - 1)) {
        capacity--;
        capacity |= capacity >> 1;
        capacity |= capacity >> 2;
        capacity |= capacity >> 4;
        capacity |= capacity >> 8;
        capacity |= capacity >> 16;
        capacity |= capacity >> 32;
        capacity++;
    }
    MN_ASSERT(!(capacity & (capacity - 1)));

    int ret = MN_ERROR_NOMEM;
    q->buffer = NULL;
    q->buffer = MN_MEM_ACQUIRE(sizeof(*q->buffer) * (1 + capacity));
    MN_GUARD_NULL(q->buffer);

    mn_atomic_store(&q->head, 0);
    mn_atomic_store(&q->tail, 0);
    q->tail_cache = 0;
    q->capacity = capacity;
    q->mask = capacity - 1;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_queue_spsc_cleanup(mn_queue_spsc_t *q)
{
    MN_ASSERT(q);
    MN_MEM_RELEASE(q->buffer);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_queue_spsc_count(mn_queue_spsc_t *q)
{
    MN_ASSERT(q);
    const uint64_t head = mn_atomic_load_explicit(&q->head, MN_ATOMIC_RELAXED);
    const uint64_t tail = mn_atomic_load(&q->tail);
    return (head - tail);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_queue_spsc_capacity(mn_queue_spsc_t *q)
{
    MN_ASSERT(q);
    return q->capacity;
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_empty(mn_queue_spsc_t *q)
{
    return (mn_queue_spsc_count(q) == 0);
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_full(mn_queue_spsc_t *q)
{
    return (mn_queue_spsc_count(q) == q->capacity);
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_push(mn_queue_spsc_t *q, void *ptr)
{
    MN_ASSERT(q);

    const uint64_t head = mn_atomic_load_explicit(&q->head, MN_ATOMIC_RELAXED);
    const uint64_t tail = mn_atomic_load(&q->tail);
    const uint64_t size = head - tail;

    if (size == q->capacity) return MN_QUEUE_FULL;

    q->buffer[head & q->mask] = (uintptr_t)ptr;
    mn_atomic_store(&q->head, head + 1);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_peek(mn_queue_spsc_t *q, void **out_ptr)
{
    MN_ASSERT(q);
    MN_ASSERT(out_ptr);

    *out_ptr = NULL;

    const uint64_t tail = mn_atomic_load_explicit(&q->tail, MN_ATOMIC_RELAXED);
    const uint64_t head = mn_atomic_load(&q->head);

    if (head == tail) return MN_QUEUE_EMPTY;

    *out_ptr = (void *)q->buffer[tail & q->mask];
    q->tail_cache = tail;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_pop(mn_queue_spsc_t *q)
{
    MN_ASSERT(q);

    const uint64_t tail = mn_atomic_load_explicit(&q->tail, MN_ATOMIC_RELAXED);
    const uint64_t head = mn_atomic_load(&q->head);

    if (head == tail) return MN_QUEUE_EMPTY;

    mn_atomic_store(&q->tail, tail + 1);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_queue_spsc_pop_cached(mn_queue_spsc_t *q)
{
    mn_atomic_store(&q->tail, q->tail_cache + 1);
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_pop_back(mn_queue_spsc_t *q, void **out_ptr)
{
    MN_ASSERT(q);
    MN_ASSERT(out_ptr);

    *out_ptr = NULL;

    const uint64_t tail = mn_atomic_load_explicit(&q->tail, MN_ATOMIC_RELAXED);
    const uint64_t head = mn_atomic_load(&q->head);

    if (head == tail) return MN_QUEUE_EMPTY;

    *out_ptr = (void *)q->buffer[tail & q->mask];

    mn_atomic_store(&q->tail, tail + 1);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_queue_spsc_pop_all(mn_queue_spsc_t *q, void **out_ptr, uint64_t *out_count)
{
    MN_ASSERT(q);
    MN_ASSERT(out_ptr);
    MN_ASSERT(out_count);
    MN_ASSERT(*out_count > 0);

    const uint64_t mask = q->mask;
    const uint64_t tail = mn_atomic_load_explicit(&q->tail, MN_ATOMIC_RELAXED);
    const uint64_t head = mn_atomic_load(&q->head);
    const uint64_t size = (head - tail);

    if (!size) return MN_QUEUE_EMPTY;

    if (size < *out_count) *out_count = size;
    for (uint64_t off = 0; off < *out_count; off++) {
        out_ptr[off] = (void *)q->buffer[(tail + off) & q->mask];
    }

    mn_atomic_store_explicit(&q->tail, tail + *out_count, MN_ATOMIC_RELAXED);

    return MN_SUCCESS;
}
