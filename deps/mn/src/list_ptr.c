#include "mn/list_ptr.h"

#include "mn/allocator.h"
#include "mn/error.h"

// --------------------------------------------------------------------------------------------------------------
int mn_list_ptr_setup(mn_list_ptr_t *list, size_t capacity)
{
    MN_GUARD_NULL(list);
    memset(list, 0, sizeof(*list));

    list->capacity = capacity;
    MN_GUARD_NULL(list->data = MN_MEM_ACQUIRE(list->capacity * sizeof(*list->data)));

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_list_ptr_cleanup(mn_list_ptr_t *list)
{
    if (!list) return;
    MN_MEM_RELEASE(list->data);
}

// --------------------------------------------------------------------------------------------------------------
int mn_list_ptr_push_back(mn_list_ptr_t *list, void *item)
{
    MN_ASSERT(list);
    MN_ASSERT(list->data);
    MN_ASSERT(item);

    MN_GUARD(list->index >= list->capacity);
    list->data[list->index++] = (uintptr_t)item;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_list_ptr_pop_back(mn_list_ptr_t *list, void **item)
{
    MN_ASSERT(list);
    MN_ASSERT(list->data);
    MN_ASSERT(item);

    MN_GUARD(list->index == 0);
    *item = (void *)list->data[--list->index];

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_list_ptr_count(mn_list_ptr_t *list)
{
    MN_ASSERT(list);
    return list->index;
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_list_ptr_capacity(mn_list_ptr_t *list)
{
    MN_ASSERT(list);
    return list->capacity;
}

// --------------------------------------------------------------------------------------------------------------
void mn_list_ptr_clear(mn_list_ptr_t *list)
{
    MN_ASSERT(list);
    list->index = 0;
}

// --------------------------------------------------------------------------------------------------------------
void *mn_list_ptr_get(mn_list_ptr_t *list, uint64_t index)
{
    MN_ASSERT(list);
    MN_ASSERT(list->data[index]);
    return (void *)list->data[index];
}

// --------------------------------------------------------------------------------------------------------------
void mn_list_ptr_swap(mn_list_ptr_t *list, uint64_t index1, uint64_t index2)
{
    MN_ASSERT(list);

    uintptr_t tmp = list->data[index1];
    list->data[index1] = list->data[index2];
    list->data[index2] = tmp;
}

// --------------------------------------------------------------------------------------------------------------
int mn_list_ptr_remove(mn_list_ptr_t *list, size_t index)
{
    MN_ASSERT(list);

    MN_GUARD(list->index == 0);

    list->data[index] = list->data[--list->index];

    return MN_SUCCESS;
}
