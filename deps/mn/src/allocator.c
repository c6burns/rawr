#include "mn/allocator.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mn/error.h"

// private ------------------------------------------------------------------------------------------------------
void *mn_aws_default_acquire(struct aws_allocator *allocator, size_t size)
{
    return malloc(size);
}

// private ------------------------------------------------------------------------------------------------------
void mn_aws_default_release(struct aws_allocator *allocator, void *ptr)
{
    if (!ptr) return;
    free(ptr);
}

aws_allocator_t mn_aws_default_allocator = {
    .mem_acquire = mn_aws_default_acquire,
    .mem_release = mn_aws_default_release,
    .mem_realloc = NULL,
    .impl = NULL,
};

// private ------------------------------------------------------------------------------------------------------
void *mn_allocator_default_acquire(const struct mn_allocator_s *allocator, size_t sz)
{
    uint8_t *mem;
    if (!(mem = aws_mem_acquire(&mn_aws_default_allocator, sz))) return NULL;

    return (void *)mem;
}

// private ------------------------------------------------------------------------------------------------------
void mn_allocator_default_release(const struct mn_allocator_s *allocator, void *ptr)
{
    if (!ptr) return;

    aws_mem_release(&mn_aws_default_allocator, ptr);
}

// private ------------------------------------------------------------------------------------------------------
void mn_allocator_default_release_ptr(const struct mn_allocator_s *allocator, void **ptr)
{
    if (!ptr) return;
    if (!*ptr) return;

    aws_mem_release(&mn_aws_default_allocator, *ptr);

    *ptr = NULL;
}

mn_allocator_t mn_default_allocator = {
    .acquire_fn = mn_allocator_default_acquire,
    .release_fn = mn_allocator_default_release,
    .release_ptr_fn = mn_allocator_default_release_ptr,
    .config = {
        .priv = NULL,
    },
    .priv = &mn_aws_default_allocator,
};

// --------------------------------------------------------------------------------------------------------------
mn_allocator_t *mn_allocator_new(void)
{
    mn_allocator_t *allocator;
    if (!(allocator = aws_mem_acquire(&mn_aws_default_allocator, sizeof(mn_allocator_t)))) return NULL;
    return allocator;
}

// --------------------------------------------------------------------------------------------------------------
void mn_allocator_delete(mn_allocator_t **ptr_allocator)
{
    if (!ptr_allocator) return;
    if (!*ptr_allocator) return;
    aws_mem_release(&mn_aws_default_allocator, *ptr_allocator);
    *ptr_allocator = NULL;
}

// --------------------------------------------------------------------------------------------------------------
int mn_allocator_setup(mn_allocator_t *allocator, const mn_allocator_config_t *config, mn_allocator_aquire_fn acquire_fn, mn_allocator_release_fn release_fn)
{
    if (!allocator) return MN_ERROR;

    if (config) {
        memcpy(&allocator->config, config, sizeof(mn_allocator_config_t));
    }

    if (acquire_fn)
        allocator->acquire_fn = acquire_fn;
    else
        allocator->acquire_fn = mn_allocator_default_acquire;

    if (release_fn)
        allocator->release_fn = release_fn;
    else
        allocator->release_fn = mn_allocator_default_release;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_allocator_cleanup(mn_allocator_t *allocator)
{
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void *mn_allocator_acquire(const mn_allocator_t *allocator, size_t sz)
{
    if (!allocator) return NULL;
    if (!allocator->acquire_fn) return NULL;

    return allocator->acquire_fn(allocator, sz);
}

// --------------------------------------------------------------------------------------------------------------
void mn_allocator_release(const mn_allocator_t *allocator, void *ptr)
{
    if (!allocator) return;
    if (!allocator->release_fn) return;

    allocator->release_fn(allocator, ptr);
}

// --------------------------------------------------------------------------------------------------------------
void mn_allocator_release_ptr(const mn_allocator_t *allocator, void **ptr)
{
    if (!allocator) return;
    if (!allocator->release_fn) return;

    allocator->release_ptr_fn(allocator, ptr);
}
