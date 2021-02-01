#ifndef RAWR_NET_H
#define RAWR_NET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Platform.h"

RAWR_API int RAWR_CALL rawr_Net_Setup();
RAWR_API void RAWR_CALL rawr_Net_Cleanup();

#ifdef __cplusplus
}
#endif

#endif
