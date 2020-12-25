#include "mn/uuid.h"

#include "aws/common/uuid.h"

#include "mn/allocator.h"
#include "mn/error.h"
#include "mn/log.h"

// --------------------------------------------------------------------------------------------------------------
mn_uuid_t *mn_uuid_new(void)
{
    return MN_MEM_ACQUIRE(sizeof(mn_uuid_t));
}

// --------------------------------------------------------------------------------------------------------------
void mn_uuid_delete(mn_uuid_t **ptr_uuid)
{
    MN_MEM_RELEASE_PTR((void **)ptr_uuid);
}

// --------------------------------------------------------------------------------------------------------------
int mn_uuid_generate(mn_uuid_t *uuid)
{
    int ret;
    if (!uuid) return MN_ERROR;
    if ((ret = aws_uuid_init((struct aws_uuid *)uuid))) mn_log_error("failed to generate uuid");
    return ret;
}

// --------------------------------------------------------------------------------------------------------------
int mn_uuid_clear(mn_uuid_t *uuid)
{
    if (!uuid) return MN_ERROR;
    memset(uuid, 0, sizeof(mn_uuid_t));
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_uuid_compare(mn_uuid_t *uuid1, mn_uuid_t *uuid2)
{
    if (!uuid1 && !uuid2) return MN_SUCCESS;
    if (!uuid1 || !uuid2) return MN_ERROR;

    return aws_uuid_equals((struct aws_uuid *)uuid1, (struct aws_uuid *)uuid2);
}
