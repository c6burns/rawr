#include "rawr/Audio.h"
#include "rawr/Call.h"
#include "rawr/Error.h"
#include "rawr/Endpoint.h"
#include "rawr/Net.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

rawr_Call *rawrcall = NULL;

int main(void)
{
    RAWR_GUARD_CLEANUP(rawr_Audio_Setup());

    RAWR_GUARD_CLEANUP(rawr_Net_Setup());

    RAWR_GUARD_CLEANUP(rawr_Call_Setup(&rawrcall, "sip:sip.tlm.partners", "sip:1001@sip.tlm.partners", "ChrisBurns", "1001", "422423"));

    RAWR_GUARD_CLEANUP(rawr_Call_Start(rawrcall, "sip:3300@sip.tlm.partners")); // 48khz conference

    mn_thread_sleep_s(20);

    RAWR_GUARD_CLEANUP(rawr_Call_Stop(rawrcall));

    //RAWR_GUARD_CLEANUP(rawr_Call_Start(rawrcall, "sip:9196@sip.tlm.partners")); // echo test

    //mn_thread_sleep_s(20);
    //RAWR_GUARD_CLEANUP(rawr_Call_BlockOnCall(rawrcall));

    //RAWR_GUARD_CLEANUP(rawr_Call_Stop(rawrcall));

    //RAWR_GUARD_CLEANUP(rawr_Call_Start(rawrcall, "sip:3300@sip.tlm.partners")); // 48khz conference

    //mn_thread_sleep_s(20);

    //RAWR_GUARD_CLEANUP(rawr_Call_Stop(rawrcall));

    rawr_Call_Cleanup(rawrcall);

    rawr_Net_Cleanup();

    rawr_Audio_Cleanup();

    return 0;

cleanup:
    mn_log_error("ERROR");

    rawr_Net_Cleanup();

    rawr_Audio_Cleanup();

    return 1;
}
