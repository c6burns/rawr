#ifndef MN_UUID_H
#define MN_UUID_H

#include <stdint.h>

typedef struct mn_uuid_s {
    uint8_t uuid_data[16];
} mn_uuid_t;

mn_uuid_t *mn_uuid_new(void);
void mn_uuid_delete(mn_uuid_t **ptr_uuid);
int mn_uuid_generate(mn_uuid_t *uuid);
int mn_uuid_clear(mn_uuid_t *uuid);
int mn_uuid_compare(mn_uuid_t *uuid1, mn_uuid_t *uuid2);
#define mn_uuid_cmp(uuid1, uuid2) mn_uuid_compare(uuid1, uuid2)

#endif
