#include "mn/atomic.h"
#include "mn/error.h"
#include "aws/common/atomics.h"

MN_STATIC_ASSERT(sizeof(mn_atomic_t) == sizeof(struct aws_atomic_var));

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_atomic_load(const volatile mn_atomic_t *a)
{
    return aws_atomic_load_int_explicit((const volatile struct aws_atomic_var *)a, MN_ATOMIC_ACQUIRE);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_atomic_load_explicit(const volatile mn_atomic_t *a, int mem_order)
{
    return aws_atomic_load_int_explicit((const volatile struct aws_atomic_var *)a, mem_order);
}

// --------------------------------------------------------------------------------------------------------------
void *mn_atomic_load_ptr(const volatile mn_atomic_t *a)
{
    return aws_atomic_load_ptr_explicit((const volatile struct aws_atomic_var *)a, MN_ATOMIC_ACQUIRE);
}

// --------------------------------------------------------------------------------------------------------------
void *mn_atomic_load_ptr_explicit(const volatile mn_atomic_t *a, int mem_order)
{
    return aws_atomic_load_ptr_explicit((const volatile struct aws_atomic_var *)a, mem_order);
}

// --------------------------------------------------------------------------------------------------------------
void mn_atomic_store(volatile mn_atomic_t *a, uint64_t val)
{
    aws_atomic_store_int_explicit((volatile struct aws_atomic_var *)a, val, MN_ATOMIC_RELEASE);
}

// --------------------------------------------------------------------------------------------------------------
void mn_atomic_store_explicit(volatile mn_atomic_t *a, uint64_t val, int mem_order)
{
    aws_atomic_store_int_explicit((volatile struct aws_atomic_var *)a, val, mem_order);
}

// --------------------------------------------------------------------------------------------------------------
void mn_atomic_store_ptr(volatile mn_atomic_t *a, void *val)
{
    aws_atomic_store_ptr_explicit((volatile struct aws_atomic_var *)a, val, MN_ATOMIC_RELEASE);
}

// --------------------------------------------------------------------------------------------------------------
void mn_atomic_store_ptr_explicit(volatile mn_atomic_t *a, void *val, int mem_order)
{
    aws_atomic_store_ptr_explicit((volatile struct aws_atomic_var *)a, val, mem_order);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_atomic_fetch_add(volatile mn_atomic_t *a, uint64_t val)
{
    return aws_atomic_fetch_add_explicit((volatile struct aws_atomic_var *)a, val, MN_ATOMIC_ACQ_REL);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_atomic_store_fetch_add(volatile mn_atomic_t *a, uint64_t val, int mem_order)
{
    return aws_atomic_fetch_add_explicit((volatile struct aws_atomic_var *)a, val, MN_ATOMIC_ACQ_REL);
}
