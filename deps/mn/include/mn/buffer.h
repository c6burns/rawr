#ifndef MN_BUFFER_H
#define MN_BUFFER_H

#include <stdint.h>

#include "aws/common/byte_buf.h"
#include "aws/common/byte_order.h"

#include "mn/error.h"
#include "mn/mutex.h"

#if MN_PLATFORM_WINDOWS
#    define MN_BUFLEN_CAST(sz) ((unsigned int)sz)
#else
#    define MN_BUFLEN_CAST(sz) (sz)
#endif

// forwards
struct mn_buffer_pool_s;

typedef struct mn_buffer_span_s {
    size_t len;
    uint8_t *ptr;
} mn_buffer_span_t;

typedef struct mn_buffer_s {
    struct mn_buffer_pool_s *pool;
    struct aws_byte_buf buf;
    struct aws_byte_cursor pos;
    size_t capacity;
} mn_buffer_t;

int mn_buffer_setup(mn_buffer_t *buffer, void *src, size_t capacity);

int mn_buffer_read(mn_buffer_t *buffer, void *dst_buffer, size_t len);
int mn_buffer_read_seek(mn_buffer_t *buffer, mn_buffer_span_t *span);
int mn_buffer_read_skip(mn_buffer_t *buffer, size_t len);
int mn_buffer_read_u8(mn_buffer_t *buffer, uint8_t *out_val);
int mn_buffer_read_be16(mn_buffer_t *buffer, uint16_t *out_val);
int mn_buffer_read_be32(mn_buffer_t *buffer, uint32_t *out_val);
int mn_buffer_read_be64(mn_buffer_t *buffer, uint64_t *out_val);
int mn_buffer_read_buffer(mn_buffer_t *buffer, mn_buffer_t *dst_buffer, size_t len);
int mn_buffer_write(mn_buffer_t *buffer, const void *src_buffer, size_t len);
int mn_buffer_write_u8(mn_buffer_t *buffer, const uint8_t val);
int mn_buffer_write_be16(mn_buffer_t *buffer, const uint16_t val);
int mn_buffer_write_be32(mn_buffer_t *buffer, const uint32_t val);
int mn_buffer_write_be64(mn_buffer_t *buffer, const uint64_t val);
int mn_buffer_write_buffer(mn_buffer_t *buffer, mn_buffer_t *src_buffer, size_t len);

void mn_buffer_read_reset(mn_buffer_t *buffer);
void mn_buffer_write_reset(mn_buffer_t *buffer);
void mn_buffer_reset(mn_buffer_t *buffer);

size_t mn_buffer_length(mn_buffer_t *buffer);
size_t mn_buffer_read_length(mn_buffer_t *buffer);
int mn_buffer_set_length(mn_buffer_t *buffer, size_t len);
int mn_buffer_add_length(mn_buffer_t *buffer, size_t len);
size_t mn_buffer_remaining(mn_buffer_t *buffer);
size_t mn_buffer_capacity(mn_buffer_t *buffer);

void *mn_buffer_write_ptr(mn_buffer_t *buffer);
void *mn_buffer_read_ptr(mn_buffer_t *buffer);

int mn_buffer_release(mn_buffer_t *buffer);

#endif
