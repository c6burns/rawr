#ifndef MN_ALLOCATOR_H
#define MN_ALLOCATOR_H

#include <stdlib.h>

#include "aws/common/common.h"

#define MN_MEM_DEBUG 0
#define MN_MEM_1K 1024
#define MN_MEM_1M MN_MEM_1K * 1024
#define MN_MEM_1G MN_MEM_1M * 1024

#if MN_MEM_DEBUG
#    define MN_MEM_ACQUIRE(sz) aws_mem_acquire(&mn_aws_default_allocator, sz, __FILENAME__, __LINE__, __FUNCTION__);
#    define MN_MEM_RELEASE(ptr) aws_mem_release(&mn_aws_default_allocator, ptr, __FILENAME__, __LINE__, __FUNCTION__);
#else
#    define MN_MEM_ACQUIRE(sz) mn_allocator_acquire(&mn_default_allocator, sz)
#    define MN_MEM_RELEASE(ptr) mn_allocator_release(&mn_default_allocator, ptr)
#    define MN_MEM_RELEASE_PTR(ptr) mn_allocator_release_ptr(&mn_default_allocator, ptr)
#endif

typedef struct mn_allocator_config_s {
    void *priv;
} mn_allocator_config_t;

struct mn_allocator_s;
#if MN_MEM_DEBUG
typedef void *(*mn_allocator_aquire_fn)(const struct mn_allocator_s *allocator, size_t sz, const char *file_name, const int line, const char *function_name);
typedef void (*mn_allocator_release_fn)(const struct mn_allocator_s *allocator, void *ptr, const char *file_name, const int line, const char *function_name);
typedef void (*mn_allocator_release_ptr_fn)(const struct mn_allocator_s *allocator, void **ptr, const char *file_name, const int line, const char *function_name);
#else
typedef void *(*mn_allocator_aquire_fn)(const struct mn_allocator_s *allocator, size_t sz);
typedef void (*mn_allocator_release_fn)(const struct mn_allocator_s *allocator, void *ptr);
typedef void (*mn_allocator_release_ptr_fn)(const struct mn_allocator_s *allocator, void **ptr);
#endif

typedef struct aws_allocator aws_allocator_t;
typedef struct mn_allocator_s {
    mn_allocator_aquire_fn acquire_fn;
    mn_allocator_release_fn release_fn;
    mn_allocator_release_ptr_fn release_ptr_fn;
    mn_allocator_config_t config;
    void *priv;
} mn_allocator_t;

mn_allocator_t *mn_allocator_new(void);
void mn_allocator_delete(mn_allocator_t **ptr_allocator);
int mn_allocator_setup(mn_allocator_t *allocator, const mn_allocator_config_t *config, mn_allocator_aquire_fn acquire_fn, mn_allocator_release_fn release_fn);
int mn_allocator_cleanup(mn_allocator_t *allocator);

#if MN_MEM_DEBUG
void *mn_allocator_acquire(const mn_allocator_t *allocator, size_t sz, const char *file_name, const int line, const char *function_name);
void mn_allocator_release(const mn_allocator_t *allocator, void *ptr, const char *file_name, const int line, const char *function_name);
void mn_allocator_release_ptr(const mn_allocator_t *allocator, void **ptr, const char *file_name, const int line, const char *function_name);
#else
void *mn_allocator_acquire(const mn_allocator_t *allocator, size_t sz);
void mn_allocator_release(const mn_allocator_t *allocator, void *ptr);
void mn_allocator_release_ptr(const mn_allocator_t *allocator, void **ptr);
#endif

// these are now private within the translation unit
//void *mn_aws_default_acquire(struct aws_allocator *allocator, size_t size);
//void *mn_aws_default_realloc(struct aws_allocator *allocator, void **ptr, size_t oldsize, size_t newsize);
//void mn_aws_default_release(struct aws_allocator *allocator, void *ptr);

// this provides a default allocator we can use right away without creating anything custom
mn_allocator_t mn_default_allocator;

aws_allocator_t mn_aws_default_allocator;

#endif
