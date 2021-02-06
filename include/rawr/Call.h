#ifndef RAWR_CALL_H
#define RAWR_CALL_H

#include "rawr/Platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rawr_CallState {
    rawr_CallState_None,
    rawr_CallState_Starting,
    rawr_CallState_Started,
    rawr_CallState_Connecting,
    rawr_CallState_Ready,
    rawr_CallState_Progressing,
    rawr_CallState_Media,
    rawr_CallState_Connected,
    rawr_CallState_Stopping,
    rawr_CallState_Stopped,
} rawr_CallState;

typedef enum rawr_CallError {
    rawr_CallError_None,
} rawr_CallError;

typedef struct rawr_Call rawr_Call;

RAWR_API rawr_CallState RAWR_CALL rawr_Call_State(rawr_Call *call);

RAWR_API int RAWR_CALL rawr_Call_Setup(rawr_Call **out_call, const char *sipRegistrar, const char *sipURI, const char *sipName, const char *sipUsername, const char *sipPassword);
RAWR_API void RAWR_CALL rawr_Call_Cleanup(rawr_Call *call);

/* dial */
RAWR_API int RAWR_CALL rawr_Call_Start(rawr_Call *call, const char *sipInviteURI);

/* hangup */
RAWR_API int RAWR_CALL rawr_Call_Stop(rawr_Call *call);

RAWR_API int RAWR_CALL rawr_Call_BlockOnCall(rawr_Call *call);

RAWR_API double RAWR_CALL rawr_Call_InputLevel(rawr_Call *call);
RAWR_API double RAWR_CALL rawr_Call_OutputLevel(rawr_Call *call);

#ifdef __cplusplus
}
#endif

#endif
