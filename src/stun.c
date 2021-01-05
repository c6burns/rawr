#include "rawr/stun.h"

#include "rawr/error.h"

#include "mn/log.h"
#include "mn/thread.h"

#include "re.h"

struct rawr_StunResponse {
    int returnValue;
    rawr_Endpoint *endpoint;
};

// --------------------------------------------------------------------------------------------------------------
void stun_handler(struct stun_msg *msg, void *arg)
{
    int abc = 123;
}

// --------------------------------------------------------------------------------------------------------------
void stun_response_handler(int err, uint16_t scode, const char *reason, const struct stun_msg *msg, void *arg)
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
int rawr_StunClient_BindingRequest(rawr_Endpoint *stunServer, rawr_Endpoint *out_endpoint)
{
    struct stun *re_stun;
    struct sa sa_srv;
    struct rawr_StunResponse resp = {
        .returnValue = rawr_Error,
        .endpoint = out_endpoint,
    };

    memset(out_endpoint, 0, sizeof(*out_endpoint));

    RAWR_GUARD_CLEANUP(sa_set_sa(&sa_srv, rawr_Endpoint_SockAddr(stunServer)));

    RAWR_GUARD_CLEANUP(stun_alloc(&re_stun, NULL, stun_handler, NULL));

    RAWR_GUARD_CLEANUP(stun_request(NULL, re_stun, IPPROTO_UDP, NULL, &sa_srv, 0, STUN_METHOD_BINDING, NULL, 0, false, stun_response_handler, &resp, 1, STUN_ATTR_SOFTWARE, "RAWR!"));

    RAWR_GUARD_CLEANUP(re_main(NULL));

    return rawr_Success;

cleanup:
    return rawr_Error;
}
