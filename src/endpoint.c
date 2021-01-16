#include "rawr/endpoint.h"

#include "rawr/error.h"
#include "rawr/platform.h"

#include "aws/common/byte_order.h"
#include "aws/common/byte_buf.h"

// --------------------------------------------------------------------------------------------------------------
uint16_t rawr_Endpoint_AF(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (uint16_t)endpoint->addr4.af;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Endpoint_SetAF(rawr_Endpoint *endpoint, rawr_SockAF af)
{
    RAWR_ASSERT(endpoint);
    endpoint->addr4.af = (RAWR_AF_TYPE)af;
}

// --------------------------------------------------------------------------------------------------------------
struct sockaddr *rawr_Endpoint_SockAddr(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (struct sockaddr *)endpoint;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_SetSockAddr(rawr_Endpoint *endpoint, void *sockaddr_void)
{
    RAWR_ASSERT(endpoint);

    int rv = rawr_Error;
    struct sockaddr_storage *sockaddr = sockaddr_void;

    memset(endpoint, 0, sizeof(*endpoint));
    if (sockaddr->ss_family == AF_INET6) {
        memcpy(endpoint, (struct sockaddr_in6 *)sockaddr, sizeof(struct sockaddr_in6));
    } else if (sockaddr->ss_family == AF_INET) {
        memcpy(endpoint, (struct sockaddr_in *)sockaddr, sizeof(struct sockaddr_in));
    } else {
        return rawr_Error;
    }
    if (sockaddr->ss_family == AF_INET) {
        memcpy(&endpoint->addr4, sockaddr, sizeof(endpoint->addr4));
        rv = rawr_Success;
    } else if (sockaddr->ss_family == AF_INET6) {
        memcpy(&endpoint->addr6, sockaddr, sizeof(endpoint->addr6));
        rv = rawr_Success;
    }

    return rv;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Endpoint_SetShorts(rawr_Endpoint *endpoint, uint16_t port, uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5, uint16_t s6, uint16_t s7)
{
    RAWR_ASSERT(endpoint);
    rawr_Endpoint_SetAF(endpoint, AF_INET6);
    rawr_Endpoint_SetPort(endpoint, port);
    *(uint16_t *)&endpoint->addr6.addr[0] = aws_hton16(s0);
    *(uint16_t *)&endpoint->addr6.addr[2] = aws_hton16(s1);
    *(uint16_t *)&endpoint->addr6.addr[4] = aws_hton16(s2);
    *(uint16_t *)&endpoint->addr6.addr[6] = aws_hton16(s3);
    *(uint16_t *)&endpoint->addr6.addr[8] = aws_hton16(s4);
    *(uint16_t *)&endpoint->addr6.addr[10] = aws_hton16(s5);
    *(uint16_t *)&endpoint->addr6.addr[12] = aws_hton16(s6);
    *(uint16_t *)&endpoint->addr6.addr[14] = aws_hton16(s7);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Endpoint_SetBytes(rawr_Endpoint *endpoint, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint16_t port)
{
    RAWR_ASSERT(endpoint);
    rawr_Endpoint_SetAF(endpoint, rawr_SockAF_IPv4);
    rawr_Endpoint_SetPort(endpoint, port);
    *((uint8_t *)&endpoint->addr4.addr + 0) = b0;
    *((uint8_t *)&endpoint->addr4.addr + 1) = b1;
    *((uint8_t *)&endpoint->addr4.addr + 2) = b2;
    *((uint8_t *)&endpoint->addr4.addr + 3) = b3;
}

// --------------------------------------------------------------------------------------------------------------
uint8_t rawr_Endpoint_B0(rawr_Endpoint *endpoint)
{
    return *((uint8_t *)&endpoint->addr4.addr + 0);
}

// --------------------------------------------------------------------------------------------------------------
uint8_t rawr_Endpoint_B1(rawr_Endpoint *endpoint)
{
    return *((uint8_t *)&endpoint->addr4.addr + 1);
}

// --------------------------------------------------------------------------------------------------------------
uint8_t rawr_Endpoint_B2(rawr_Endpoint *endpoint)
{
    return *((uint8_t *)&endpoint->addr4.addr + 2);
}

// --------------------------------------------------------------------------------------------------------------
uint8_t rawr_Endpoint_B3(rawr_Endpoint *endpoint)
{
    return *((uint8_t *)&endpoint->addr4.addr + 3);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_String(rawr_Endpoint *endpoint, uint16_t *port, char *buf, int buf_len)
{
    RAWR_ASSERT(endpoint);
    RAWR_ASSERT(port);
    RAWR_ASSERT(buf);
    *port = 0;
    memset(buf, 0, buf_len);

    char ipbuf[255];
    memset(ipbuf, 0, 255);

    struct sockaddr_in6 *sa = (struct sockaddr_in6 *)endpoint;
    if (rawr_Endpoint_Is6(endpoint)) {
        inet_ntop(AF_INET6, sa, ipbuf, 255);
        *port = ntohs(sa->sin6_port);
    } else if (rawr_Endpoint_Is4(endpoint)) {
        struct sockaddr_in *sa = (struct sockaddr_in *)endpoint;
        snprintf(ipbuf, 255, "%u.%u.%u.%u", rawr_Endpoint_B0(endpoint), rawr_Endpoint_B1(endpoint), rawr_Endpoint_B2(endpoint), rawr_Endpoint_B3(endpoint));
        *port = ntohs(sa->sin_port);
    } else {
        return rawr_Error;
    }

    sprintf(buf, "%s", ipbuf);
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_In4(rawr_Endpoint *endpoint, uint32_t *out_in4)
{
    RAWR_ASSERT(endpoint);

    RAWR_GUARD(!rawr_Endpoint_Is4(endpoint));

    *out_in4 = aws_ntoh32(endpoint->addr4.addr);

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Endpoint_SetIn4(rawr_Endpoint *endpoint, uint32_t in4)
{
    RAWR_ASSERT(endpoint);

    RAWR_GUARD_CLEANUP(!rawr_Endpoint_Is4(endpoint));

    endpoint->addr4.addr = aws_hton32(in4);

    return;

cleanup:
    return;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Port(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    RAWR_ASSERT(endpoint->addr4.port == endpoint->addr6.port);
    return aws_ntoh16(endpoint->addr4.port);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Endpoint_SetPort(rawr_Endpoint *endpoint, uint16_t port)
{
    RAWR_ASSERT(endpoint);
    endpoint->addr4.port = aws_hton16(port);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Is4(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (rawr_Endpoint_AF(endpoint) == (uint16_t)rawr_SockAF_IPv4);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Is6(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (rawr_Endpoint_AF(endpoint) == (uint16_t)rawr_SockAF_IPv6);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Size(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    if (rawr_Endpoint_Is4(endpoint)) return sizeof(rawr_SockAddr4);
    if (rawr_Endpoint_Is6(endpoint)) return sizeof(rawr_SockAddr6);
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_IsEqual(rawr_Endpoint *endpoint, void *sockaddr)
{
    rawr_Endpoint *addr = (rawr_Endpoint *)sockaddr;
    if (rawr_Endpoint_AF(addr) != rawr_Endpoint_AF(endpoint)) return false;

    if (rawr_Endpoint_Is6(addr)) {
        struct sockaddr_in6 *ep_addr6 = (struct sockaddr_in6 *)endpoint;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (memcmp(endpoint, addr, sizeof(struct sockaddr_in6))) return false;
        return true;
    } else if (rawr_Endpoint_Is4(addr)) {
        if (((rawr_Endpoint *)sockaddr)->addr4.addr != endpoint->addr4.addr) return false;
        if (((rawr_Endpoint *)sockaddr)->addr4.port != endpoint->addr4.port) return false;
        return true;
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Hash(rawr_Endpoint *endpoint, uint64_t *out_hash)
{
    RAWR_ASSERT(out_hash);

    *out_hash = 0;
    if (rawr_Endpoint_Is4(endpoint)) {
        struct aws_byte_cursor bc = aws_byte_cursor_from_array(endpoint, sizeof(*endpoint));
        *out_hash = aws_hash_byte_cursor_ptr(&bc);
        return rawr_Success;
    } else if (rawr_Endpoint_Is6(endpoint)) {
        *out_hash = 0x0000000000000000ULL & endpoint->addr4.port;
        *out_hash <<= 32;
        *out_hash |= endpoint->addr4.addr;
        return rawr_Success;
    }

    return rawr_Error;
}
