#include "rawr/pa.h"

#include "portaudio.h"
#include <math.h>
#include <stdint.h>

#ifdef WIN32
#    include <windows.h>

#    if PA_USE_ASIO
#        include "pa_asio.h"
#    endif
#endif

#include "mn/log.h"

static double rawr_pa_samplerates[] = {
    8000.0,
    9600.0,
    11025.0,
    12000.0,
    16000.0,
    22050.0,
    24000.0,
    32000.0,
    44100.0,
    48000.0,
    88200.0,
    96000.0,
    192000.0,
    -1 /* negative terminated  list */
};

typedef struct rawr_pa_private_s {
    int32_t default_input;
    int32_t default_output;
    char default_input_name[RAWR_PA_NAME_MAX];
    char default_output_name[RAWR_PA_NAME_MAX];
    int32_t initialized;
} rawr_pa_private_t;

rawr_pa_private_t pa_priv = { 0 };

// --------------------------------------------------------------------------------------------------------------
int rawr_pa_setup(void)
{
    int i, numDevices, defaultDisplayed;
    const PaDeviceInfo *deviceInfo;
    PaStreamParameters inputParameters, outputParameters;
    PaError err;

    if (pa_priv.initialized == 1) return paNoError;

    if (pa_priv.initialized > 1) {
        memset(&pa_priv, 0, sizeof(pa_priv));
    }
    pa_priv.initialized = 1;

    return Pa_Initialize();
}

// --------------------------------------------------------------------------------------------------------------
int rawr_pa_cleanup(void)
{
    if (pa_priv.initialized != 1) return paNoError;

    pa_priv.initialized = 0;
    return Pa_Terminate();
}

// --------------------------------------------------------------------------------------------------------------
int rawr_pa_dev_default_input(void)
{
    return Pa_GetDefaultInputDevice();
}

// --------------------------------------------------------------------------------------------------------------
int rawr_pa_dev_default_output(void)
{
    return Pa_GetDefaultOutputDevice();
}

// --------------------------------------------------------------------------------------------------------------
const char *rawr_pa_dev_name(int id)
{
    return Pa_GetDeviceInfo(id)->name;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_pa_dev_rate_supported(int rate)
{
    return 0;
}
