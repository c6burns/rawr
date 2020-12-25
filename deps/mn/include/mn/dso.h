#ifndef MN_DSO_H
#define MN_DSO_H

typedef enum mn_dso_state_e {
    MN_DSO_STATE_UNLOADED,
    MN_DSO_STATE_LOADED,
    MN_DSO_STATE_INVALID,
} mn_dso_state_t;

typedef struct mn_dso_s {
    void *lib;
    mn_dso_state_t state;
} mn_dso_t;

mn_dso_state_t mn_dso_state(mn_dso_t *dso);
int mn_dso_setup(mn_dso_t *dso, const char *so_name);
int mn_dso_symbol(mn_dso_t *dso, const char *sym_name, void **out_fn_ptr);
int mn_dso_cleanup(mn_dso_t *dso);

#endif
