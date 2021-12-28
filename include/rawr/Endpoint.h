#ifndef RAWR_ENDPOINT_H
#define RAWR_ENDPOINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rawr/Platform.h"

#if RAWR_PLATFORM_OSX || RAWR_PLATFORM_PS5
#    define RAWR_AF_TYPE uint8_t
#else
#    define RAWR_AF_TYPE uint16_t
#endif

typedef enum rawr_SockAF {
    rawr_SockAF_IPv4 = AF_INET,
    rawr_SockAF_IPv6 = AF_INET6,
} rawr_SockAF;

typedef struct rawr_SockAddr4 {
#if RAWR_PLATFORM_OSX || RAWR_PLATFORM_PS5
    uint8_t len;
#endif
    RAWR_AF_TYPE af;
    uint16_t port;
    uint32_t addr;
    uint8_t pad[8];
} rawr_SockAddr4;

RAWR_STATIC_ASSERT(sizeof(struct sockaddr_in) == sizeof(rawr_SockAddr4));

typedef struct rawr_SockAddr6 {
#if RAWR_PLATFORM_OSX || RAWR_PLATFORM_PS5
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

RAWR_API uint16_t RAWR_CALL rawr_Endpoint_AF(rawr_Endpoint *endpoint);
RAWR_API void RAWR_CALL rawr_Endpoint_SetAF(rawr_Endpoint *endpoint, rawr_SockAF af);
RAWR_API struct sockaddr * RAWR_CALL rawr_Endpoint_SockAddr(rawr_Endpoint *endpoint);
RAWR_API int RAWR_CALL rawr_Endpoint_SetSockAddr(rawr_Endpoint *endpoint, void *sockaddr_void);
RAWR_API void RAWR_CALL rawr_Endpoint_SetShorts(rawr_Endpoint *endpoint, uint16_t port, uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5, uint16_t s6, uint16_t s7);
RAWR_API void RAWR_CALL rawr_Endpoint_SetBytes(rawr_Endpoint *endpoint, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint16_t port);
RAWR_API int RAWR_CALL rawr_Endpoint_String(rawr_Endpoint *endpoint, uint16_t *port, char *buf, int buf_len);
RAWR_API int RAWR_CALL rawr_Endpoint_In4(rawr_Endpoint *endpoint, uint32_t *out_in4);
RAWR_API void RAWR_CALL rawr_Endpoint_SetIn4(rawr_Endpoint *endpoint, uint32_t in4);
RAWR_API int RAWR_CALL rawr_Endpoint_Port(rawr_Endpoint *endpoint);
RAWR_API void RAWR_CALL rawr_Endpoint_SetPort(rawr_Endpoint *endpoint, uint16_t port);
RAWR_API int RAWR_CALL rawr_Endpoint_Is4(rawr_Endpoint *endpoint);
RAWR_API int RAWR_CALL rawr_Endpoint_Is6(rawr_Endpoint *endpoint);
RAWR_API int RAWR_CALL rawr_Endpoint_Size(rawr_Endpoint *endpoint);
RAWR_API int RAWR_CALL rawr_Endpoint_IsEqual(rawr_Endpoint *endpoint, void *sockaddr);
RAWR_API int RAWR_CALL rawr_Endpoint_Hash(rawr_Endpoint *endpoint, uint64_t *out_hash);

#ifdef __cplusplus
}
#endif

#endif
