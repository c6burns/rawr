#ifndef MN_BUFFER_POOL_H
#define MN_BUFFER_POOL_H

#include <stdint.h>

#include "aws/common/byte_buf.h"
#include "aws/common/byte_order.h"

#include "mn/buffer.h"
#include "mn/mutex.h"
#include "mn/queue_spsc.h"

typedef struct mn_buffer_pool_s {
    uint64_t blocks_inuse;
    uint64_t bytes_inuse;
    uint64_t block_size;
    uint64_t block_count;
    mn_buffer_t *mn_buffers;
    uint8_t *allocation;
    mn_queue_spsc_t mn_buffers_free;
} mn_buffer_pool_t;

int mn_buffer_pool_setup(mn_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void mn_buffer_pool_cleanup(mn_buffer_pool_t *pool);

int mn_buffer_pool_push(mn_buffer_pool_t *pool, mn_buffer_t *buffer);
int mn_buffer_pool_peek(mn_buffer_pool_t *pool, mn_buffer_t **out_buffer);
int mn_buffer_pool_pop(mn_buffer_pool_t *pool);
void mn_buffer_pool_pop_cached(mn_buffer_pool_t *pool);
int mn_buffer_pool_pop_back(mn_buffer_pool_t *pool, mn_buffer_t **out_buffer);

#endif
