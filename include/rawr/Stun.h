#ifndef RAWR_STUN_H
#define RAWR_STUN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rawr/Endpoint.h"
#include "rawr/Platform.h"

typedef struct rawr_StunClient rawr_StunClient;

RAWR_API int RAWR_CALL rawr_StunClient_Setup(rawr_StunClient **out_client);
RAWR_API void RAWR_CALL rawr_StunClient_Cleanup(rawr_StunClient *client);

RAWR_API int RAWR_CALL rawr_StunClient_LocalEndpoint(rawr_StunClient *client, rawr_Endpoint *out_endpoint);
RAWR_API int RAWR_CALL rawr_StunClient_BindingRequest(rawr_StunClient *client, rawr_Endpoint *stunServer, rawr_Endpoint *out_endpoint);

#ifdef __cplusplus
}
#endif

#endif
