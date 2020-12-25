#ifndef MN_TIME_H
#define MN_TIME_H

#include <stdint.h>

#define MN_TSTAMP_S 1
#define MN_TSTAMP_MS 1000
#define MN_TSTAMP_US 1000000
#define MN_TSTAMP_NS 1000000000

uint64_t mn_tstamp(void);
uint64_t mn_tstamp_convert(uint64_t tstamp, uint32_t from, uint32_t to);

#endif