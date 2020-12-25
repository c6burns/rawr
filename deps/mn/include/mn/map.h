#ifndef MN_MAP_H
#define MN_MAP_H

#include <stdint.h>

typedef struct mn_map_s {
    void *priv;
    uint64_t capacity;
} mn_map_t;

int mn_map_setup(mn_map_t *map, uint64_t capacity);
void mn_map_cleanup(mn_map_t *map);
int mn_map_get(mn_map_t *map, void *key, void **out_value);
int mn_map_set(mn_map_t *map, void *key, void *value);
int mn_map_remove(mn_map_t *map, void *key);
void mn_map_clear(mn_map_t *map);

#endif
