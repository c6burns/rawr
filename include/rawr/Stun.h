#ifndef RAWR_STUN_H
#define RAWR_STUN_H

#include "rawr/Endpoint.h"

typedef struct rawr_StunClient rawr_StunClient;

int rawr_StunClient_Setup(rawr_StunClient **out_client);
void rawr_StunClient_Cleanup(rawr_StunClient *client);

int rawr_StunClient_BindingRequest(rawr_StunClient *client, rawr_Endpoint *stunServer, rawr_Endpoint *out_endpoint);

#endif
