#ifndef MN_LIST_PTR_H
#define MN_LIST_PTR_H

#include <stddef.h>
#include <stdint.h>

typedef struct mn_list_ptr_s {
    uintptr_t *data;
    uint64_t capacity;
    uint64_t index;
} mn_list_ptr_t;

int mn_list_ptr_setup(mn_list_ptr_t *list, size_t capacity);
void mn_list_ptr_cleanup(mn_list_ptr_t *list);
int mn_list_ptr_push_back(mn_list_ptr_t *list, void *item);
int mn_list_ptr_pop_back(mn_list_ptr_t *list, void **item);
uint64_t mn_list_ptr_count(mn_list_ptr_t *list);
uint64_t mn_list_ptr_capacity(mn_list_ptr_t *list);
void mn_list_ptr_clear(mn_list_ptr_t *list);
void *mn_list_ptr_get(mn_list_ptr_t *list, uint64_t index);
int mn_list_ptr_remove(mn_list_ptr_t *list, size_t index);

#endif
