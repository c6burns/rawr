#ifndef RAWR_BUF_H
#define RAWR_BUF_H

#include "aws/common/byte_buf.h"

typedef struct rawr_buf_s {
	struct aws_byte_buf bb;
	uint16_t pos;
} rawr_buf_t;

int rawr_buf_setup(rawr_buf_t *buf, uint16_t capacity);
void rawr_buf_cleanup(rawr_buf_t *buf);

uint16_t rawr_buf_length(rawr_buf_t *buf);
uint16_t rawr_buf_capacity(rawr_buf_t *buf);
char *rawr_buf_slice(rawr_buf_t *buf, uint16_t offset);

void rawr_buf_clear(rawr_buf_t *buf);
int rawr_buf_insert(rawr_buf_t *buf, char c);
int rawr_buf_delete(rawr_buf_t *buf);

uint16_t rawr_buf_cursor_get(rawr_buf_t *buf);
int rawr_buf_cursor_set(rawr_buf_t *buf, uint16_t pos);

#endif