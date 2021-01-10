#include "rawr/audio.h"
#include "rawr/call.h"
#include "rawr/error.h"
#include "rawr/opus.h"

#include "mn/atomic.h"
#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

typedef struct rawr_Call {
    mn_atomic_t state;
    rawr_CallError error;
    mn_atomic_t threadExiting;
    mn_thread_t thread;
    rawr_AudioStream *stream;
    rawr_Codec *encoder;
    rawr_Codec *decoder;
} rawr_Call;

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_SetState(rawr_Call *call, rawr_CallState callState)
{
    RAWR_ASSERT(call);
    mn_log_debug("%d -> %d", call->state, callState);
    mn_atomic_store(&call->state, callState);
}

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_SetExiting(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_atomic_store(&call->threadExiting, 1);
}

// private ------------------------------------------------------------------------------------------------------
int rawr_Call_Exiting(rawr_Call *call)
{
    RAWR_ASSERT(call);
    return (int)mn_atomic_load(&call->threadExiting);
}

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_Thread(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_SetState(call, rawr_CallState_Started);

    while (!rawr_Call_Exiting(call)) {
        int exitLoop = 1;

        rawr_CallState state = rawr_Call_State(call);
        switch (state) {
        case rawr_CallState_Ready:
        case rawr_CallState_Progressing:
        case rawr_CallState_Media:
        case rawr_CallState_Connected:
            exitLoop = 0;
        }

        if (exitLoop) break;

        mn_thread_sleep_ms(50);
    }
}

// --------------------------------------------------------------------------------------------------------------
rawr_CallState rawr_Call_State(rawr_Call *call)
{
    RAWR_ASSERT(call);
    return (rawr_CallState)mn_atomic_load(&call->state);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Setup(rawr_Call **out_call)
{
    RAWR_ASSERT(out_call);
    RAWR_GUARD_NULL(*out_call = MN_MEM_ACQUIRE(sizeof(**out_call)));
    memset(*out_call, 0, sizeof(**out_call));

    RAWR_GUARD(mn_thread_setup(&(*out_call)->thread));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Call_Cleaup(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_thread_cleanup(&call->thread);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Start(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_SetState(call, rawr_CallState_Starting);
    RAWR_GUARD(mn_thread_launch(&call->thread, rawr_Call_Thread, call));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Stop(rawr_Call *call)
{
    rawr_Call_SetState(call, rawr_CallState_Stopping);
    RAWR_ASSERT(call);
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_BlockOnCall(rawr_Call *call)
{
    while (!rawr_Call_Exiting(call)) {
        int exitLoop = 1;

        rawr_CallState state = rawr_Call_State(call);
        switch (state) {
        case rawr_CallState_Starting:
        case rawr_CallState_Started:
        case rawr_CallState_Connecting:
        case rawr_CallState_Ready:
        case rawr_CallState_Progressing:
        case rawr_CallState_Media:
        case rawr_CallState_Connected:
            exitLoop = 0;
        }

        if (exitLoop) break;

        mn_thread_sleep_ms(50);
    }
    return rawr_Success;
}
