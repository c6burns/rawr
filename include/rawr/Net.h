#ifndef RAWR_NET_H
#define RAWR_NET_H

#include "Platform.h"

#ifdef __cplusplus
extern "C" {
#endif

RAWR_API int RAWR_CALL rawr_Net_Setup();
RAWR_API void RAWR_CALL rawr_Net_Cleanup();

#ifdef __cplusplus
}
#endif

#endif
