#ifndef RAWR_STUN_H
#define RAWR_STUN_H

#include "rawr/endpoint.h"

typedef struct rawr_StunClient rawr_StunClient;

int rawr_StunClient_BindingRequest(rawr_Endpoint *stunServer, rawr_Endpoint *out_endpoint);

#endif
