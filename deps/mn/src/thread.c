#include "mn/thread.h"

#include "aws/common/thread.h"

#include "mn/allocator.h"
#include "mn/error.h"

// --------------------------------------------------------------------------------------------------------------
void mn_thread_sleep(uint64_t ns)
{
    aws_thread_current_sleep(ns);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_thread_id(void)
{
    return aws_thread_current_thread_id();
}

// --------------------------------------------------------------------------------------------------------------
int mn_thread_setup(mn_thread_t *thread)
{
    MN_ASSERT(thread);
    return aws_thread_init((struct aws_thread *)thread, aws_default_allocator());
}

// --------------------------------------------------------------------------------------------------------------
int mn_thread_launch(mn_thread_t *thread, void (*func)(void *arg), void *arg)
{
    MN_ASSERT(thread);
    mn_thread_setup(thread);
    return aws_thread_launch((struct aws_thread *)thread, func, arg, aws_default_thread_options());
}

// --------------------------------------------------------------------------------------------------------------
uint64_t mn_thread_get_id(mn_thread_t *thread)
{
    MN_ASSERT(thread);
    return aws_thread_get_id((struct aws_thread *)thread);
}

enum mn_thread_state mn_thread_get_state(mn_thread_t *thread)
{
    MN_ASSERT(thread);
    return (enum mn_thread_state)aws_thread_get_detach_state((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
int mn_thread_join(mn_thread_t *thread)
{
    MN_ASSERT(thread);
    return aws_thread_join((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
void mn_thread_cleanup(mn_thread_t *thread)
{
    MN_ASSERT(thread);
    aws_thread_clean_up((struct aws_thread *)thread);
}
