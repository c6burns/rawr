#include "rawr/error.h"
#include "rawr/stun.h"

#include "mn/log.h"
#include "mn/thread.h"


int main(void)
{
    rawr_Endpoint epStunServ, epExternal;

    RAWR_GUARD_CLEANUP(libre_init());

    rawr_Endpoint_SetBytes(&epStunServ, 74, 125, 197, 127, 19302);

    rawr_StunClient_BindingRequest(&epStunServ, &epExternal);

    char ipstr[255];
    uint16_t port;
    rawr_Endpoint_String(&epExternal, &port, ipstr, 255);
    mn_log_info("%s:%u", ipstr, port);

    libre_close();

    return 0;

cleanup:
    mn_log_error("ERROR");
    libre_close();

    return 1;
}
