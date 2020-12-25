#ifndef MN_ATOMIC_H
#define MN_ATOMIC_H

#include <stdint.h>

#define MN_ATOMIC_INIT(x)               \
    {                                   \
        .value = (void *)(uintptr_t)(x) \
    }

typedef struct mn_atomic_s {
    void *value;
} mn_atomic_t;

// atomic memory barriers
#define MN_ATOMIC_RELAXED 0
#define MN_ATOMIC_ACQUIRE 2
#define MN_ATOMIC_RELEASE 3
#define MN_ATOMIC_ACQ_REL 4
#define MN_ATOMIC_SEQ_CST 5

uint64_t mn_atomic_load(const volatile mn_atomic_t *a);
uint64_t mn_atomic_load_explicit(const volatile mn_atomic_t *a, int mem_order);

void *mn_atomic_load_ptr(const volatile mn_atomic_t *a);
void *mn_atomic_load_ptr_explicit(const volatile mn_atomic_t *a, int mem_order);

void mn_atomic_store(volatile mn_atomic_t *a, uint64_t val);
void mn_atomic_store_explicit(volatile mn_atomic_t *a, uint64_t val, int mem_order);

void mn_atomic_store_ptr(volatile mn_atomic_t *a, void *val);
void mn_atomic_store_ptr_explicit(volatile mn_atomic_t *a, void *val, int mem_order);

uint64_t mn_atomic_fetch_add(volatile mn_atomic_t *a, uint64_t val);
uint64_t mn_atomic_store_fetch_add(volatile mn_atomic_t *a, uint64_t val, int mem_order);

#endif
