#include "rawr/audio.h"
#include "rawr/error.h"

#include "portaudio.h"
#include <stdint.h>

#include "mn/allocator.h"
#include "mn/error.h"
#include "mn/log.h"

const double rawr_AudioRateList[] = {
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
    -1.0,
};

typedef struct rawr_AudioPrivate {
    uint64_t initialized;
    rawr_AudioDeviceId defaultInputId;
    rawr_AudioDeviceId defaultOutputId;
    rawr_AudioDevice *deviceList;
    rawr_AudioDeviceId deviceCount;
} rawr_AudioPrivate;

typedef struct rawr_AudioDevicePrivate {
    const PaDeviceInfo *deviceInfo;
} rawr_AudioDevicePrivate;

typedef struct rawr_AudioStreamPrivate {
    const PaStream *stream;
    const PaStreamParameters inputParameters;
    const PaStreamParameters outParameters;
} rawr_AudioStreamPrivate;

static rawr_AudioPrivate audio_priv = {0};

// --------------------------------------------------------------------------------------------------------------
int rawr_Audio_Setup(void)
{
    int errCode = 0;
    PaError err;

    if (audio_priv.initialized) return RAWR_OK;

    errCode = -1;
    MN_GUARD_CLEANUP(err = Pa_Initialize());
    audio_priv.initialized = 1;

    errCode = -2;
    MN_GUARD_CLEANUP((audio_priv.deviceCount = Pa_GetDeviceCount()) < 0);

    errCode = -3;
    MN_GUARD_NULL_CLEANUP(audio_priv.deviceList = MN_MEM_ACQUIRE(audio_priv.deviceCount * sizeof(*audio_priv.deviceList)));
    audio_priv.defaultInputId = Pa_GetDefaultInputDevice();
    audio_priv.defaultOutputId = Pa_GetDefaultOutputDevice();
    for (int d = 0; d < audio_priv.deviceCount; d++) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(d);
        rawr_AudioDevice *dev = audio_priv.deviceList + d;
        rawr_AudioDevicePrivate *priv;

        errCode = -4;
        MN_GUARD_NULL_CLEANUP(priv = MN_MEM_ACQUIRE(sizeof(*priv)));
        priv->deviceInfo = deviceInfo;
        dev->priv = priv;

        dev->id = d;
        dev->props = rawr_AudioDeviceProps_None;
        if (d == audio_priv.defaultInputId || d == audio_priv.defaultOutputId)
            dev->props |= rawr_AudioDeviceProps_Default;

        PaStreamParameters inputParameters;
        PaStreamParameters *pInParams = &inputParameters;
        inputParameters.device = d;
        inputParameters.channelCount = deviceInfo->maxInputChannels;
        inputParameters.sampleFormat = paInt16;
        inputParameters.suggestedLatency = 0;
        inputParameters.hostApiSpecificStreamInfo = NULL;
        if (inputParameters.channelCount <= 0)
            pInParams = NULL;
        if (inputParameters.channelCount > 0)
            dev->props |= rawr_AudioDeviceProps_Input;
        if (inputParameters.channelCount > 1)
            dev->props |= rawr_AudioDeviceProps_InputStereo;

        PaStreamParameters outputParameters;
        PaStreamParameters *pOutParams = &outputParameters;
        outputParameters.device = d;
        outputParameters.channelCount = deviceInfo->maxOutputChannels;
        outputParameters.sampleFormat = paInt16;
        outputParameters.suggestedLatency = 0;
        outputParameters.hostApiSpecificStreamInfo = NULL;
        if (outputParameters.channelCount <= 0)
            pOutParams = NULL;
        if (outputParameters.channelCount > 0)
            dev->props |= rawr_AudioDeviceProps_Output;
        if (outputParameters.channelCount > 1)
            dev->props |= rawr_AudioDeviceProps_OutputStereo;

        dev->rates = rawr_AudioRateFlags_None;
        for (int r = 0; rawr_AudioRateList[r] > 0.0; r++) {

            if (Pa_IsFormatSupported(pInParams, pOutParams, rawr_AudioRateList[r]) == paFormatIsSupported) {
                dev->rates |= 1 << r;
            }
        }
    }

    return rawr_Success;

cleanup:
    switch (errCode) {
    case -1:
        mn_log_error("Pa_Initialize %d: %s", err, Pa_GetErrorText(err));
        break;
    case -2:
        mn_log_error("Pa_GetDeviceCount %d: %s", err, Pa_GetErrorText(err));
        break;
    case -3:
        mn_log_error("deviceList allocation");
        break;
    case -4:
        mn_log_error("device allocation");
        break;
    }
    
    return err;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Audio_Cleanup(void)
{
    PaError err;

    if (audio_priv.initialized != 1) return RAWR_OK;

    for (int d = 0; d < audio_priv.deviceCount; d++) {
        MN_MEM_RELEASE((audio_priv.deviceList + d)->priv);
    }
    MN_MEM_RELEASE(audio_priv.deviceList);

    audio_priv.initialized = 0;
    MN_GUARD_CLEANUP(err = Pa_Terminate());

    return rawr_Success;

cleanup:
    mn_log_error("Pa_Initialize returned %d: %s", err, Pa_GetErrorText(err));
    return err;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_Get(rawr_AudioDeviceId id)
{
    RAWR_ASSERT(id > 0 && id < audio_priv.deviceCount);
    return NULL;
}

// private ------------------------------------------------------------------------------------------------------
rawr_AudioDevicePrivate *rawr_AudioDevice_GetPriv(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return (rawr_AudioDevicePrivate *)dev->priv;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_GetDefaultInput(void)
{
    MN_GUARD_CLEANUP(audio_priv.defaultInputId < 0 || audio_priv.defaultInputId >= audio_priv.deviceCount);
    return audio_priv.deviceList + audio_priv.defaultInputId;

cleanup:
    return NULL;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_GetDefaultOutput(void)
{
    MN_GUARD_CLEANUP(audio_priv.defaultOutputId < 0 || audio_priv.defaultOutputId >= audio_priv.deviceCount);
    return audio_priv.deviceList + audio_priv.defaultOutputId;

cleanup:
    return NULL;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDeviceId rawr_AudioDevice_GetId(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->id;
}

// --------------------------------------------------------------------------------------------------------------
const char *rawr_AudioDevice_GetName(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    rawr_AudioDevicePrivate *priv = rawr_AudioDevice_GetPriv(dev);
    return priv->deviceInfo->name;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDeviceProps rawr_AudioDevice_GetProps(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->props;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioRateFlags rawr_AudioDevice_GetRates(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->rates;
}
