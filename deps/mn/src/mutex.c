#include "mn/mutex.h"

#include "aws/common/mutex.h"
#include "mn/error.h"

// --------------------------------------------------------------------------------------------------------------
int mn_mutex_setup(mn_mutex_t *mtx)
{
    MN_ASSERT(mtx);
    return aws_mutex_init((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int mn_mutex_lock(mn_mutex_t *mtx)
{
    MN_ASSERT(mtx);
    return aws_mutex_lock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int mn_mutex_unlock(mn_mutex_t *mtx)
{
    MN_ASSERT(mtx);
    return aws_mutex_unlock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
void mn_mutex_cleanup(mn_mutex_t *mtx)
{
    MN_ASSERT(mtx);
    aws_mutex_clean_up((struct aws_mutex *)mtx);
}
