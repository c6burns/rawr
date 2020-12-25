#include "mn/buffer_pool.h"

#include "mn/allocator.h"
#include "mn/buffer.h"
#include "mn/error.h"
#include "mn/log.h"

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_pool_setup(mn_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size)
{
    MN_ASSERT(pool);
    MN_ASSERT(block_count > 0);
    MN_ASSERT(block_size > 0);

    pool->block_count = block_count;
    pool->block_size = block_size;
    pool->blocks_inuse = 0;
    pool->bytes_inuse = 0;
    pool->mn_buffers = NULL;
    pool->allocation = NULL;

    MN_GUARD_NULL_CLEANUP(pool->allocation = MN_MEM_ACQUIRE(block_count * block_size));
    MN_GUARD_NULL_CLEANUP(pool->mn_buffers = MN_MEM_ACQUIRE(block_count * sizeof(*pool->mn_buffers)));
    MN_GUARD_CLEANUP(mn_queue_spsc_setup(&pool->mn_buffers_free, block_count));

    mn_buffer_t *mn_buffer;
    for (uint64_t i = 0; i < block_count; i++) {
        mn_buffer = &pool->mn_buffers[i];

        uint8_t *buf = pool->allocation + ((block_count - 1 - i) * block_size);

        MN_GUARD_CLEANUP(mn_buffer_setup(mn_buffer, buf, block_size));
        mn_buffer->pool = pool;

        MN_GUARD_CLEANUP(mn_queue_spsc_push(&pool->mn_buffers_free, mn_buffer));
    }

    return MN_SUCCESS;

cleanup:
    MN_MEM_RELEASE(pool->mn_buffers);
    MN_MEM_RELEASE(pool->allocation);

    return MN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void mn_buffer_pool_cleanup(mn_buffer_pool_t *pool)
{
    if (!pool) return;

    pool->blocks_inuse = 0;
    pool->bytes_inuse = 0;

    mn_queue_spsc_cleanup(&pool->mn_buffers_free);
    MN_MEM_RELEASE(pool->mn_buffers);
    MN_MEM_RELEASE(pool->allocation);
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_pool_push(mn_buffer_pool_t *pool, mn_buffer_t *buffer)
{
    MN_ASSERT(pool);
    MN_ASSERT(buffer);

    MN_GUARD(mn_queue_spsc_push(&pool->mn_buffers_free, buffer));

    pool->blocks_inuse--;
    pool->bytes_inuse -= pool->block_size;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_pool_peek(mn_buffer_pool_t *pool, mn_buffer_t **out_buffer)
{
    MN_ASSERT(pool);
    MN_ASSERT(out_buffer);

    *out_buffer = NULL;

    MN_GUARD(mn_queue_spsc_peek(&pool->mn_buffers_free, (void **)out_buffer));
    MN_ASSERT(*out_buffer);

    mn_buffer_reset(*out_buffer);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_pool_pop(mn_buffer_pool_t *pool)
{
    MN_ASSERT(pool);

    MN_GUARD(mn_queue_spsc_pop(&pool->mn_buffers_free));

    pool->blocks_inuse++;
    pool->bytes_inuse += pool->block_size;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_buffer_pool_pop_cached(mn_buffer_pool_t *pool)
{
    MN_ASSERT(pool);

    mn_queue_spsc_pop_cached(&pool->mn_buffers_free);

    pool->blocks_inuse++;
    pool->bytes_inuse += pool->block_size;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_pool_pop_back(mn_buffer_pool_t *pool, mn_buffer_t **out_buffer)
{
    MN_ASSERT(pool);
    MN_ASSERT(out_buffer);

    *out_buffer = NULL;

    MN_GUARD(mn_queue_spsc_pop_back(&pool->mn_buffers_free, (void **)out_buffer));
    MN_ASSERT(*out_buffer);

    pool->blocks_inuse++;
    pool->bytes_inuse += pool->block_size;

    mn_buffer_reset(*out_buffer);

    return MN_SUCCESS;
}
