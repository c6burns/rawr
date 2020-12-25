#include "mn/dso.h"
#include "mn/allocator.h"
#include "mn/error.h"
#include "uv.h"

// --------------------------------------------------------------------------------------------------------------
mn_dso_state_t mn_dso_state(mn_dso_t *dso)
{
    MN_ASSERT(dso);
    return dso->state;
}

// private ------------------------------------------------------------------------------------------------------
static inline void mn_dso_state_set(mn_dso_t *dso, mn_dso_state_t state)
{
    MN_ASSERT(dso);
    dso->state = state;
}

// private ------------------------------------------------------------------------------------------------------
static inline bool mn_dso_state_valid(mn_dso_state_t state)
{
    switch (state) {
    case MN_DSO_STATE_UNLOADED:
    case MN_DSO_STATE_LOADED:
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------------------------------------------
int mn_dso_setup(mn_dso_t *dso, const char *so_name)
{
    MN_ASSERT(dso);
    MN_GUARD(mn_dso_state(dso) != MN_DSO_STATE_UNLOADED);
    MN_GUARD_NULL(dso->lib = MN_MEM_ACQUIRE(sizeof(uv_lib_t)));
    MN_GUARD(uv_dlopen(so_name, (uv_lib_t *)dso->lib));
    mn_dso_state_set(dso, MN_DSO_STATE_LOADED);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_dso_symbol(mn_dso_t *dso, const char *sym_name, void **out_fn_ptr)
{
    MN_ASSERT(dso);
    MN_ASSERT(out_fn_ptr);
    MN_GUARD(mn_dso_state(dso) != MN_DSO_STATE_LOADED);
    MN_GUARD(uv_dlsym((uv_lib_t *)dso->lib, sym_name, out_fn_ptr));
    MN_GUARD_NULL(*out_fn_ptr);
    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_dso_cleanup(mn_dso_t *dso)
{
    MN_ASSERT(dso);
    MN_GUARD(mn_dso_state(dso) != MN_DSO_STATE_LOADED);
    uv_dlclose((uv_lib_t *)dso->lib);
    mn_dso_state_set(dso, MN_DSO_STATE_UNLOADED);
    MN_MEM_RELEASE(dso->lib);
    return MN_SUCCESS;
}
