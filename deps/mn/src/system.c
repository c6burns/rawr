#include "mn/system.h"

#include "mn/allocator.h"
#include "mn/error.h"

typedef struct mn_system_priv_s {
    uint32_t cpu_count;
} mn_system_priv_t;

// --------------------------------------------------------------------------------------------------------------
int mn_system_setup(mn_system_t *system)
{
    MN_ASSERT(system);

    mn_system_priv_t *priv = NULL;
    MN_GUARD_NULL(priv = MN_MEM_ACQUIRE(sizeof(*priv)));

    memset(priv, 0, sizeof(*priv));
    system->priv = priv;

    int cpu_count = priv->cpu_count = 0;

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void mn_system_cleanup(mn_system_t *system)
{
    MN_ASSERT(system);

    mn_system_priv_t *priv = system->priv;

    MN_MEM_RELEASE(system->priv);
}

// --------------------------------------------------------------------------------------------------------------
uint32_t mn_system_cpu_count(mn_system_t *system)
{
    MN_ASSERT(system && system->priv);

    return ((mn_system_priv_t *)system->priv)->cpu_count;

    return MN_SUCCESS;
}
