#include "rawr/buf.h"

#include "mn/error.h"

// --------------------------------------------------------------------------------------------------------------
int rawr_buf_setup(rawr_buf_t *buf, uint16_t capacity)
{
	MN_ASSERT(buf);
	MN_GUARD(aws_byte_buf_init(&buf->bb, aws_default_allocator(), capacity));
	buf->bb.buffer[0] = '\0';
	buf->pos = 0;
	return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_buf_cleanup(rawr_buf_t *buf)
{
	MN_ASSERT(buf);
	aws_byte_buf_clean_up(&buf->bb);
}

// --------------------------------------------------------------------------------------------------------------
uint16_t rawr_buf_length(rawr_buf_t *buf)
{
	MN_ASSERT(buf);
	return (uint16_t)buf->bb.len;
}

// --------------------------------------------------------------------------------------------------------------
uint16_t rawr_buf_capacity(rawr_buf_t *buf)
{
	MN_ASSERT(buf);
	return (uint16_t)buf->bb.capacity;
}

// --------------------------------------------------------------------------------------------------------------
char *rawr_buf_slice(rawr_buf_t *buf, uint16_t offset)
{
	MN_ASSERT(buf && aws_byte_buf_is_valid(&buf->bb));
	MN_GUARD(offset > buf->bb.len);
	return (buf->bb.buffer + offset);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_buf_clear(rawr_buf_t *buf)
{
	MN_ASSERT(buf);
	aws_byte_buf_reset(&buf->bb, false);
	buf->pos = 0;
	buf->bb.buffer[0] = '\0';
}

// --------------------------------------------------------------------------------------------------------------
int rawr_buf_insert(rawr_buf_t *buf, char c)
{
	MN_ASSERT(buf && aws_byte_buf_is_valid(&buf->bb));
	MN_GUARD(buf->pos >= buf->bb.capacity - 1);
	const uint16_t copylen = (uint16_t)buf->bb.len - buf->pos;
	if (copylen) memmove(buf->bb.buffer + buf->pos + 1, buf->bb.buffer + buf->pos, copylen);
	buf->bb.buffer[buf->pos] = c;
	buf->bb.len++;
	buf->pos++;
	buf->bb.buffer[buf->bb.len] = '\0';
	return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_buf_delete(rawr_buf_t *buf)
{ 
	MN_ASSERT(buf && aws_byte_buf_is_valid(&buf->bb));
	const uint16_t copylen = (uint16_t)buf->bb.len - buf->pos - 1;
	MN_GUARD(copylen > buf->bb.len);
	if (copylen) memmove(buf->bb.buffer + buf->pos, buf->bb.buffer + buf->pos + 1, copylen);
	buf->bb.len--;
	buf->bb.buffer[buf->bb.len] = '\0';
	return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
uint16_t rawr_buf_cursor_get(rawr_buf_t *buf)
{
	MN_ASSERT(buf);
	return buf->pos;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_buf_cursor_set(rawr_buf_t *buf, uint16_t pos)
{
	MN_ASSERT(buf);
	MN_GUARD(pos > buf->bb.len);
	buf->pos = pos;
	return MN_SUCCESS;
}