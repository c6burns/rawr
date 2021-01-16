#include "rawr/error.h"
#include "rawr/net.h"
#include "rawr/stun.h"

#include "mn/log.h"
#include "mn/thread.h"


int main(void)
{
    int err;
    rawr_Endpoint epStunServ, epExternal;

    RAWR_GUARD_CLEANUP(err = rawr_Net_Setup());

    rawr_Endpoint_SetBytes(&epStunServ, 74, 125, 197, 127, 19302);

    RAWR_GUARD_CLEANUP(err = rawr_StunClient_BindingRequest(&epStunServ, &epExternal));

    char ipstr[255];
    uint16_t port;
    RAWR_GUARD_CLEANUP(err = rawr_Endpoint_String(&epExternal, &port, ipstr, 255));
    mn_log_info("%s:%u", ipstr, port);

    err = 0;

cleanup:
    if (err) mn_log_error("ERROR");
    rawr_Net_Cleanup();

    return 1;
}
