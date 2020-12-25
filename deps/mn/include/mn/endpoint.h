#ifndef MN_ENDPOINT_H
#define MN_ENDPOINT_H

#include "mn/error.h"

#include <stdbool.h>
#include <stdint.h>

#if MN_PLATFORM_OSX
#    define MN_AF_TYPE uint8_t
#else
#    define MN_AF_TYPE uint16_t
#endif

#define MN_ENDPOINT_MAX_SIZE 28

typedef struct mn_sockaddr4_s {
#if MN_PLATFORM_OSX
    uint8_t len;
#endif
    MN_AF_TYPE family;
    uint16_t port;
    uint32_t addr;
} mn_sockaddr4_t;

typedef struct mn_sockaddr6_s {
#if MN_PLATFORM_OSX
    uint8_t len;
#endif
    MN_AF_TYPE family;
    uint16_t port;
    uint32_t flowinfo;
    uint8_t addr[16];
    uint32_t scope_id;
} mn_sockaddr6_t;

typedef union mn_sockaddr_u {
    mn_sockaddr4_t addr4;
    mn_sockaddr6_t addr6;
} mn_sockaddr_t;

typedef mn_sockaddr_t mn_endpoint_t;

bool mn_endpoint_is_ipv4(const mn_endpoint_t *endpoint);
bool mn_endpoint_is_ipv6(const mn_endpoint_t *endpoint);
uint16_t mn_endpoint_af_get(const mn_endpoint_t *endpoint);
void mn_endpoint_af_set(mn_endpoint_t *endpoint, uint16_t af);
uint16_t mn_endpoint_port_get(mn_endpoint_t *endpoint);
void mn_endpoint_from_short(mn_endpoint_t *endpoint, uint16_t port, uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5, uint16_t s6, uint16_t s7);
void mn_endpoint_from_byte(mn_endpoint_t *endpoint, uint16_t port, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);
void mn_endpoint_port_set(mn_endpoint_t *endpoint, uint16_t port);
int mn_endpoint_string_get(mn_endpoint_t *endpoint, uint16_t *port, char *buf, int buf_len);
int mn_endpoint_from_string(mn_endpoint_t *endpoint, const char *ip, uint16_t port);
int mn_endpoint_set_ip4(mn_endpoint_t *endpoint, const char *ip, uint16_t port);
int mn_endpoint_set_ip6(mn_endpoint_t *endpoint, const char *ip, uint16_t port);
int mn_endpoint_convert_from(mn_endpoint_t *endpoint, void *sockaddr);
bool mn_endpoint_equal_addr(mn_endpoint_t *endpoint, void *sockaddr);
int mn_endpoint_get_hash(mn_endpoint_t *endpoint, uint64_t *out_hash);

#endif
