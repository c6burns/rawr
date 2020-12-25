#include "mn/buffer.h"

#include "mn/allocator.h"
#include "mn/buffer_pool.h"
#include "mn/error.h"
#include "mn/log.h"

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_setup(mn_buffer_t *buffer, void *src, size_t capacity)
{
    MN_ASSERT(buffer && src && capacity);

    buffer->pool = NULL;
    buffer->buf = aws_byte_buf_from_empty_array(src, capacity);
    buffer->pos = aws_byte_cursor_from_buf(&buffer->buf);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read(mn_buffer_t *buffer, void *dst_buffer, size_t len)
{
    MN_ASSERT(buffer);
    MN_ASSERT(dst_buffer);
    if (!aws_byte_cursor_read(&buffer->pos, dst_buffer, len)) return MN_ERROR;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_seek(mn_buffer_t *buffer, mn_buffer_span_t *span)
{
    MN_ASSERT(buffer);
    MN_ASSERT(span && span->ptr && span->len);
    MN_ASSERT((span->ptr + span->len) < (buffer->buf.buffer + buffer->buf.capacity));
    buffer->pos.ptr = span->ptr;
    buffer->pos.len = span->len;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_skip(mn_buffer_t *buffer, size_t len)
{
    MN_ASSERT(buffer);
    struct aws_byte_cursor span = aws_byte_cursor_advance(&buffer->pos, len);
    if (span.ptr && span.len) {
        return MN_SUCCESS;
    }
    return MN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_u8(mn_buffer_t *buffer, uint8_t *out_val)
{
    MN_ASSERT(buffer);
    MN_ASSERT(out_val);
    if (!aws_byte_cursor_read_u8(&buffer->pos, out_val)) return MN_ERROR;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_be16(mn_buffer_t *buffer, uint16_t *out_val)
{
    MN_ASSERT(buffer);
    MN_ASSERT(out_val);
    if (!aws_byte_cursor_read_be16(&buffer->pos, out_val)) return MN_ERROR;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_be32(mn_buffer_t *buffer, uint32_t *out_val)
{
    MN_ASSERT(buffer);
    MN_ASSERT(out_val);
    if (!aws_byte_cursor_read_be32(&buffer->pos, out_val)) return MN_ERROR;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_be64(mn_buffer_t *buffer, uint64_t *out_val)
{
    MN_ASSERT(buffer);
    MN_ASSERT(out_val);
    if (!aws_byte_cursor_read_be64(&buffer->pos, out_val)) return MN_ERROR;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_read_buffer(mn_buffer_t *buffer, mn_buffer_t *dst_buffer, size_t len)
{
    MN_ASSERT(buffer && dst_buffer);

    if (!len) {
        len = buffer->pos.len;
    } else if (len > buffer->pos.len) {
        return MN_ERROR;
    }

    MN_GUARD(mn_buffer_write(dst_buffer, buffer->pos.ptr, len));

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write(mn_buffer_t *buffer, const void *src_buffer, size_t len)
{
    MN_ASSERT(buffer);
    if (!aws_byte_buf_write(&buffer->buf, src_buffer, len)) return MN_ERROR;
    buffer->pos.len += len;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write_u8(mn_buffer_t *buffer, const uint8_t val)
{
    MN_ASSERT(buffer);
    if (!aws_byte_buf_write_u8(&buffer->buf, val)) return MN_ERROR;
    buffer->pos.len += sizeof(val);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write_be16(mn_buffer_t *buffer, const uint16_t val)
{
    MN_ASSERT(buffer);
    if (!aws_byte_buf_write_be16(&buffer->buf, val)) return MN_ERROR;
    buffer->pos.len += sizeof(val);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write_be32(mn_buffer_t *buffer, const uint32_t val)
{
    MN_ASSERT(buffer);
    if (!aws_byte_buf_write_be32(&buffer->buf, val)) return MN_ERROR;
    buffer->pos.len += sizeof(val);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write_be64(mn_buffer_t *buffer, const uint64_t val)
{
    MN_ASSERT(buffer);
    if (!aws_byte_buf_write_be64(&buffer->buf, val)) return MN_ERROR;
    buffer->pos.len += sizeof(val);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_write_buffer(mn_buffer_t *buffer, mn_buffer_t *src_buffer, size_t len)
{
    return mn_buffer_read_buffer(src_buffer, buffer, len);
}

// --------------------------------------------------------------------------------------------------------------
void mn_buffer_read_reset(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    buffer->pos.ptr = buffer->buf.buffer;
    buffer->pos.len = buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void mn_buffer_write_reset(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    buffer->buf.len = 0;
    mn_buffer_read_reset(buffer);
}

// --------------------------------------------------------------------------------------------------------------
void mn_buffer_reset(mn_buffer_t *buffer)
{
    mn_buffer_write_reset(buffer);
}

// --------------------------------------------------------------------------------------------------------------
size_t mn_buffer_length(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
size_t mn_buffer_read_length(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->pos.len;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_set_length(mn_buffer_t *buffer, size_t len)
{
    MN_ASSERT(buffer);
    MN_GUARD(len > (buffer->buf.capacity - buffer->buf.len));
    buffer->buf.len = len;
    buffer->pos.len = len;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_add_length(mn_buffer_t *buffer, size_t len)
{
    MN_ASSERT(buffer);
    MN_GUARD(len > (buffer->buf.capacity - buffer->buf.len));
    buffer->buf.len += len;
    buffer->pos.len += len;
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
size_t mn_buffer_remaining(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->buf.capacity - buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
size_t mn_buffer_capacity(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->pool->block_size;
}

// --------------------------------------------------------------------------------------------------------------
void *mn_buffer_write_ptr(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->buf.buffer + buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void *mn_buffer_read_ptr(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    return buffer->pos.ptr;
}

// --------------------------------------------------------------------------------------------------------------
int mn_buffer_release(mn_buffer_t *buffer)
{
    MN_ASSERT(buffer);
    MN_GUARD_NULL(buffer->pool);

    return mn_buffer_pool_push(buffer->pool, buffer);
}
