#include "rawr/Stun.h"

#include "rawr/Error.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

#include "re.h"

struct rawr_StunResponse {
    int returnValue;
    rawr_Endpoint *endpoint;
};

typedef struct rawr_StunClient {
    struct udp_sock *udpSock;
    struct stun *re_stun;
    struct sa sa_srv;
    struct sa sa_local;
} rawr_StunClient;

// private handler ----------------------------------------------------------------------------------------------
void rawr_StunClient_UdpHandler(const struct sa *src, struct mbuf *mb, void *arg)
{
    struct stun *stun = arg;
    (void)src;

    (void)stun_recv(stun, mb);
}

// private handler ----------------------------------------------------------------------------------------------
void rawr_StunClient_IndicatorHandler(struct stun_msg *msg, void *arg)
{
    (void)msg;
    (void)arg;
}

// private handler ----------------------------------------------------------------------------------------------
void rawr_StunClient_ResponseHandler(int err, uint16_t scode, const char *reason, const struct stun_msg *msg, void *arg)
{
    struct rawr_StunResponse *resp = (struct rawr_StunResponse *)arg;
    struct stun_attr *attr;
    (void)reason;

    RAWR_GUARD_CLEANUP(err || scode > 0);

    RAWR_GUARD_CLEANUP(!(attr = stun_msg_attr(msg, STUN_ATTR_XOR_MAPPED_ADDR)));

    RAWR_GUARD_CLEANUP(rawr_Endpoint_SetSockAddr(resp->endpoint, &attr->v.sa));

    resp->returnValue = rawr_Success;
    re_cancel();
    return;

cleanup:
    resp->returnValue = rawr_Error;
    re_cancel();
}

// --------------------------------------------------------------------------------------------------------------
RAWR_API int RAWR_CALL rawr_StunClient_Setup(rawr_StunClient **out_client)
{
    RAWR_ASSERT(out_client);

    int err;

    RAWR_GUARD_NULL(*out_client = MN_MEM_ACQUIRE(sizeof(**out_client)));
    memset(*out_client, 0, sizeof(**out_client));

    RAWR_GUARD_CLEANUP(stun_alloc(&(*out_client)->re_stun, NULL, rawr_StunClient_IndicatorHandler, *out_client));

    err = net_default_source_addr_get(AF_INET, &(*out_client)->sa_local);
    if (err) {
        mn_log_error("net_default_source_addr_get failed");
        goto cleanup;
    }

    err = udp_listen(&(*out_client)->udpSock, &(*out_client)->sa_local, rawr_StunClient_UdpHandler, (*out_client)->re_stun);
    if (err) {
        mn_log_error("udp_listen failed");
        goto cleanup;
    }

    return rawr_Success;

cleanup:
    MN_MEM_RELEASE(*out_client);
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
RAWR_API void RAWR_CALL rawr_StunClient_Cleanup(rawr_StunClient *client)
{
    RAWR_ASSERT(client);
    mem_deref(client->re_stun);
    mem_deref(client->udpSock);
    MN_MEM_RELEASE(client);
}

// --------------------------------------------------------------------------------------------------------------
RAWR_API int RAWR_CALL rawr_StunClient_LocalEndpoint(rawr_StunClient *client, rawr_Endpoint *out_endpoint)
{
    RAWR_ASSERT(client && out_endpoint);

    RAWR_GUARD(rawr_Endpoint_SetSockAddr(out_endpoint, &client->sa_local.u.in));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
RAWR_API int RAWR_CALL rawr_StunClient_BindingRequest(rawr_StunClient *client, rawr_Endpoint *stunServer, rawr_Endpoint *out_endpoint)
{
    RAWR_ASSERT(client);

    struct stun *re_stun;
    struct sa sa_srv;
    struct rawr_StunResponse resp;

    resp.returnValue = rawr_Error;
    resp.endpoint = out_endpoint,
    memset(out_endpoint, 0, sizeof(*out_endpoint));

    sa_init(&client->sa_srv, AF_INET);
    RAWR_GUARD_CLEANUP(sa_set_sa(&client->sa_srv, rawr_Endpoint_SockAddr(stunServer)));

    RAWR_GUARD_CLEANUP(stun_request(NULL, client->re_stun, IPPROTO_UDP, client->udpSock, &client->sa_srv, 0, STUN_METHOD_BINDING, NULL, 0, false, rawr_StunClient_ResponseHandler, &resp, 1, STUN_ATTR_SOFTWARE, "RAWR!"));

    RAWR_GUARD_CLEANUP(re_main(NULL));

    return rawr_Success;

cleanup:
    return rawr_Error;
}
