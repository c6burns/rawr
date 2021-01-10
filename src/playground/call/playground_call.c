#include "rawr/call.h"
#include "rawr/error.h"
#include "rawr/endpoint.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

rawr_Call *rawrcall = NULL;

int main(void)
{
    rawr_Endpoint epStunServ, epExternal;

    RAWR_GUARD_CLEANUP(rawr_Audio_Setup(&rawrcall));

    RAWR_GUARD_CLEANUP(libre_init());

    RAWR_GUARD_CLEANUP(rawr_Call_Setup(&rawrcall));

    RAWR_GUARD_CLEANUP(rawr_Call_Start(rawrcall));

    RAWR_GUARD_CLEANUP(rawr_Call_BlockOnCall(rawrcall));

    RAWR_GUARD_CLEANUP(rawr_Call_Stop(rawrcall));

    rawr_Call_Cleaup(rawrcall);

    libre_close();

    rawr_Audio_Cleanup();

    return 0;

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
