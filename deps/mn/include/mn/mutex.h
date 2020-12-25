#ifndef MN_MUTEX_H
#define MN_MUTEX_H

#ifndef _WIN32
#    include <pthread.h>
#endif

typedef struct mn_mutex_s {
#ifdef _WIN32
    void *mutex_handle;
#else
    pthread_mutex_t mutex_handle;
#endif
} mn_mutex_t;

int mn_mutex_setup(mn_mutex_t *mtx);
int mn_mutex_lock(mn_mutex_t *mtx);
int mn_mutex_unlock(mn_mutex_t *mtx);
void mn_mutex_cleanup(mn_mutex_t *mtx);

#endif
