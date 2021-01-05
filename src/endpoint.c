#include "rawr/endpoint.h"

#include "rawr/error.h"
#include "rawr/platform.h"

#include "aws/common/byte_order.h"

// --------------------------------------------------------------------------------------------------------------
uint16_t rawr_Endpoint_AF(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (uint16_t)endpoint->addr4.af;
}

// --------------------------------------------------------------------------------------------------------------
struct sockaddr *rawr_Endpoint_SockAddr(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (struct sockaddr *)endpoint;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_SetSockAddr(rawr_Endpoint *endpoint, struct sockaddr *sa)
{
    RAWR_ASSERT(endpoint);

    int rv = rawr_Error;

    if (sa->sa_family == AF_INET) {
        memcpy(&endpoint->addr4, sa, sizeof(endpoint->addr4));
        rv = rawr_Success;
    } else if (sa->sa_family == AF_INET6) {
        memcpy(&endpoint->addr6, sa, sizeof(endpoint->addr6));
        rv = rawr_Success;
    }

    return rv;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_In4(rawr_Endpoint *endpoint, uint32_t *out_in4)
{
    RAWR_ASSERT(endpoint);

    RAWR_GUARD(!rawr_Endpoint_Is4(endpoint));

    *out_in4 = aws_ntoh32(endpoint->addr4.addr);

    return;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_SetIn4(rawr_Endpoint *endpoint, uint32_t in4)
{
    RAWR_ASSERT(endpoint);

    RAWR_GUARD(!rawr_Endpoint_Is4(endpoint));

    endpoint->addr4.addr = aws_hton32(in4);

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
    return endpoint->addr4.port;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Is4(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (rawr_Endpoint_AF(endpoint) == (uint16_t)rawr_SockAF_IPV4);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Is6(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    return (rawr_Endpoint_AF(endpoint) == (uint16_t)rawr_SockAF_IPV6);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Endpoint_Size(rawr_Endpoint *endpoint)
{
    RAWR_ASSERT(endpoint);
    if (rawr_Endpoint_Is4(endpoint)) return sizeof(rawr_SockAddr4);
    if (rawr_Endpoint_Is6(endpoint)) return sizeof(rawr_SockAddr6);
    return rawr_Error;
}

#endif
