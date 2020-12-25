#ifndef MN_SYSTEM_H
#define MN_SYSTEM_H

#include <stdint.h>

typedef struct mn_system_s {
    void *priv;
} mn_system_t;

int mn_system_setup(mn_system_t *system);
void mn_system_cleanup(mn_system_t *system);
uint32_t mn_system_cpu_count(mn_system_t *system);

#endif
