#ifndef RAWR_ENDPOINT_H
#define RAWR_ENDPOINT_H

#include "rawr/platform.h"

#if RAWR_PLATFORM_OSX
#    define RAWR_AF_TYPE uint8_t
#else
#    define RAWR_AF_TYPE uint16_t
#endif

typedef enum rawr_SockAF {
    rawr_SockAF_IPV4 = AF_INET,
    rawr_SockAF_IPV6 = AF_INET6,
} rawr_SockAF;

typedef struct rawr_SockAddr4 {
#if RAWR_PLATFORM_OSX
    uint8_t len;
#endif
    RAWR_AF_TYPE af;
    uint16_t port;
    uint32_t addr;
    uint8_t pad[8];
} rawr_SockAddr4;

RAWR_STATIC_ASSERT(sizeof(struct sockaddr_in) == sizeof(rawr_SockAddr4));

typedef struct rawr_SockAddr6 {
#if RAWR_PLATFORM_OSX
    uint8_t len;
#endif
    RAWR_AF_TYPE af;
    uint16_t port;
    uint32_t flowinfo;
    uint8_t addr[16];
    uint32_t scope_id;
} rawr_SockAddr6;

RAWR_STATIC_ASSERT(sizeof(struct sockaddr_in6) == sizeof(rawr_SockAddr6));

typedef union rawr_Endpoint {
    rawr_SockAddr4 addr4;
    rawr_SockAddr6 addr6;
} rawr_Endpoint;

uint16_t rawr_Endpoint_AF(rawr_Endpoint *endpoint);
struct sockaddr *rawr_Endpoint_SockAddr(rawr_Endpoint *endpoint);
int rawr_Endpoint_SetSockAddr(rawr_Endpoint *endpoint, struct sockaddr *sa);
int rawr_Endpoint_In4(rawr_Endpoint *endpoint, uint32_t in4);
int rawr_Endpoint_SetIn4(rawr_Endpoint *endpoint, uint32_t in4);
int rawr_Endpoint_Port(rawr_Endpoint *endpoint);
void rawr_Endpoint_SetPort(rawr_Endpoint *endpoint, uint16_t port);
int rawr_Endpoint_Is4(rawr_Endpoint *endpoint);
int rawr_Endpoint_Is6(rawr_Endpoint *endpoint);
int rawr_Endpoint_Size(rawr_Endpoint *endpoint);

#endif
