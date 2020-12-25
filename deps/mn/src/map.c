#include "mn/map.h"

#include "aws/common/hash_table.h"

#include "mn/allocator.h"
#include "mn/error.h"

// private ------------------------------------------------------------------------------------------------------
uint64_t mn_map_key_hash_fn(const void *key)
{
    return (uint64_t)key;
}

// private ------------------------------------------------------------------------------------------------------
bool mn_map_key_cmp_fn(const void *a, const void *b)
{
    return (a == b);
}

// --------------------------------------------------------------------------------------------------------------
int mn_map_setup(mn_map_t *map, uint64_t capacity)
{
    MN_ASSERT(map);
    memset(map, 0, sizeof(*map));

    map->capacity = capacity;

    struct aws_hash_table *priv = MN_MEM_ACQUIRE(sizeof(*priv));
    MN_GUARD_NULL(priv);

    MN_GUARD_NULL_CLEANUP(aws_hash_table_init(priv, &mn_aws_default_allocator, capacity, mn_map_key_hash_fn, mn_map_key_cmp_fn, NULL, NULL));

    map->priv = priv;
cleanup:
    MN_MEM_RELEASE(priv);
    return MN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void mn_map_cleanup(mn_map_t *map)
{
    MN_ASSERT(map);
    aws_hash_table_clean_up((struct aws_hash_table *)map->priv);
    MN_MEM_RELEASE(map->priv);
}

// --------------------------------------------------------------------------------------------------------------
int mn_map_get(mn_map_t *map, void *key, void **out_value)
{
    MN_ASSERT(map);
    MN_ASSERT(map->priv);
    MN_ASSERT(out_value);

    *out_value = NULL;
    struct aws_hash_element *elem = NULL;
    MN_GUARD(aws_hash_table_find((struct aws_hash_table *)map->priv, key, &elem));

    MN_GUARD_NULL(elem);
    *out_value = elem->value;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_map_set(mn_map_t *map, void *key, void *value)
{
    MN_ASSERT(map);
    MN_ASSERT(map->priv);
    MN_ASSERT(value);

    int created = 0;
    MN_GUARD(aws_hash_table_put((struct aws_hash_table *)map->priv, key, value, &created));
    MN_GUARD(created == 0);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_map_remove(mn_map_t *map, void *key)
{
    MN_ASSERT(map);
    MN_ASSERT(map->priv);

    int removed = 0;
    MN_GUARD(aws_hash_table_remove((struct aws_hash_table *)map->priv, key, NULL, &removed));
    MN_GUARD(removed == 0);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_map_clear(mn_map_t *map)
{
    MN_ASSERT(map);
    MN_ASSERT(map->priv);
    aws_hash_table_clear((struct aws_hash_table *)map->priv);
}
