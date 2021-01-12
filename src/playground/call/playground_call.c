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

    RAWR_GUARD_CLEANUP(rawr_Audio_Setup());

    RAWR_GUARD_CLEANUP(rawr_Net_Setup());

    RAWR_GUARD_CLEANUP(rawr_Call_Setup(&rawrcall, "sip:sip.serverlynx.net", "sip:1001@serverlynx.net", "ChrisBurns", "1001", "422423"));

    RAWR_GUARD_CLEANUP(rawr_Call_Start(rawrcall));

    RAWR_GUARD_CLEANUP(rawr_Call_BlockOnCall(rawrcall));

    RAWR_GUARD_CLEANUP(rawr_Call_Stop(rawrcall));

    rawr_Call_Cleaup(rawrcall);

    rawr_Net_Cleanup();

    rawr_Audio_Cleanup();

    return 0;

cleanup:
    mn_log_error("ERROR");

    rawr_Net_Cleanup();

    rawr_Audio_Cleanup();

    return 1;
}