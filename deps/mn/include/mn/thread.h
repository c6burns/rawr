#ifndef MN_THREAD_H
#define MN_THREAD_H

#include <stdint.h>

#ifndef _WIN32
#    include <pthread.h>
#endif

#include "mn/time.h"

#define MN_TIME_MS_PER_S 1000
#define MN_TIME_US_PER_S 1000000
#define MN_TIME_NS_PER_S 1000000000

#define MN_TIME_US_PER_MS 1000
#define MN_TIME_NS_PER_MS 1000000

#define MN_TIME_NS_PER_US 1000

#define MN_THREAD_SLEEP_S(s) aws_thread_current_sleep(ms *MN_TIME_NS_PER_S)
#define MN_THREAD_SLEEP_MS(ms) aws_thread_current_sleep(ms *MN_TIME_NS_PER_MS)

#define MN_WORKERS_MAX 128

enum mn_thread_state {
    MN_THREAD_NEW,
    MN_THREAD_READY,
    MN_THREAD_JOINABLE,
    MN_THREAD_JOINED,
};

typedef struct mn_thread_s {
    struct aws_allocator *allocator;
    enum mn_thread_state detach_state;
#ifdef _WIN32
    void *thread_handle;
    unsigned long thread_id;
#else
    pthread_t thread_id;
#endif
} mn_thread_t;

void mn_thread_sleep(uint64_t ns);
#define mn_thread_sleep_s(tstamp) mn_thread_sleep(mn_tstamp_convert(tstamp, MN_TSTAMP_S, MN_TSTAMP_NS))
#define mn_thread_sleep_ms(tstamp) mn_thread_sleep(mn_tstamp_convert(tstamp, MN_TSTAMP_MS, MN_TSTAMP_NS))
#define mn_thread_sleep_us(tstamp) mn_thread_sleep(mn_tstamp_convert(tstamp, MN_TSTAMP_US, MN_TSTAMP_NS))
#define mn_thread_sleep_ns(tstamp) mn_thread_sleep(tstamp)

uint64_t mn_thread_id(void);
int mn_thread_setup(mn_thread_t *thread);
int mn_thread_launch(mn_thread_t *thread, void (*func)(void *arg), void *arg);
uint64_t mn_thread_get_id(mn_thread_t *thread);
enum mn_thread_state mn_thread_get_state(mn_thread_t *thread);
int mn_thread_join(mn_thread_t *thread);
void mn_thread_cleanup(mn_thread_t *thread);

#endif
