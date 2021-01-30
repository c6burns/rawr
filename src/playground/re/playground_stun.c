#include "rawr/Error.h"
#include "rawr/Net.h"
#include "rawr/Stun.h"

#include "mn/log.h"
#include "mn/thread.h"

rawr_StunClient *stunClient = NULL;

int main(void)
{
    char ipstr[255];
    uint16_t port;
    int err;
    rawr_Endpoint epStunServ, epExternal;

    RAWR_GUARD_CLEANUP(err = rawr_Net_Setup());

    RAWR_GUARD_CLEANUP(err = rawr_StunClient_Setup(&stunClient));

    rawr_Endpoint_SetBytes(&epStunServ, 35, 170, 178, 149, 3478);
    RAWR_GUARD_CLEANUP(err = rawr_StunClient_BindingRequest(stunClient, &epStunServ, &epExternal));
    
    RAWR_GUARD_CLEANUP(err = rawr_Endpoint_String(&epExternal, &port, ipstr, 255));
    mn_log_info("%s:%u", ipstr, port);

    rawr_Endpoint_SetBytes(&epStunServ, 54, 205, 30, 204, 3478);
    RAWR_GUARD_CLEANUP(err = rawr_StunClient_BindingRequest(stunClient, &epStunServ, &epExternal));
    
    RAWR_GUARD_CLEANUP(err = rawr_Endpoint_String(&epExternal, &port, ipstr, 255));
    mn_log_info("%s:%u", ipstr, port);

    rawr_StunClient_Cleanup(stunClient);

    err = 0;

cleanup:
    if (err) mn_log_error("ERROR");
    rawr_Net_Cleanup();

    return 1;
}
