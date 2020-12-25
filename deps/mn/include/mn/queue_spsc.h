#ifndef MN_QUEUE_SPSC_H
#define MN_QUEUE_SPSC_H

#include <stdint.h>

#include "mn/atomic.h"

#define MN_CACHE_LINE_SIZE 64

typedef struct mn_queue_spsc_s {
    uint64_t capacity;
    uint64_t mask;
    uintptr_t *buffer;
    uint8_t cache_line_pad0[MN_CACHE_LINE_SIZE - sizeof(uint64_t) - sizeof(uint64_t) - sizeof(uintptr_t *)];

    mn_atomic_t head;
    uint8_t cache_line_pad1[MN_CACHE_LINE_SIZE - sizeof(mn_atomic_t)];

    mn_atomic_t tail;
    uint8_t cache_line_pad2[MN_CACHE_LINE_SIZE - sizeof(mn_atomic_t)];

    uint64_t tail_cache;
    uint8_t cache_line_pad3[MN_CACHE_LINE_SIZE - sizeof(uint64_t)];
} mn_queue_spsc_t;

int mn_queue_spsc_setup(mn_queue_spsc_t *q, uint64_t capacity);
void mn_queue_spsc_cleanup(mn_queue_spsc_t *q);
uint64_t mn_queue_spsc_count(mn_queue_spsc_t *q);
uint64_t mn_queue_spsc_capacity(mn_queue_spsc_t *q);
int mn_queue_spsc_empty(mn_queue_spsc_t *q);
int mn_queue_spsc_full(mn_queue_spsc_t *q);
int mn_queue_spsc_push(mn_queue_spsc_t *q, void *ptr);
int mn_queue_spsc_peek(mn_queue_spsc_t *q, void **out_ptr);
int mn_queue_spsc_pop(mn_queue_spsc_t *q);
void mn_queue_spsc_pop_cached(mn_queue_spsc_t *q);
int mn_queue_spsc_pop_back(mn_queue_spsc_t *q, void **out_ptr);
int mn_queue_spsc_pop_all(mn_queue_spsc_t *q, void **out_ptr, uint64_t *out_count);

#endif
