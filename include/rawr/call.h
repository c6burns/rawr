#ifndef RAWR_CALL_H
#define RAWR_CALL_H

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

rawr_CallState rawr_Call_State(rawr_Call *call);

int rawr_Call_Setup(rawr_Call **out_call);
void rawr_Call_Cleaup(rawr_Call *call);

int rawr_Call_Start(rawr_Call *call);
int rawr_Call_Stop(rawr_Call *call);

int rawr_Call_BlockOnCall(rawr_Call *call);

#endif
