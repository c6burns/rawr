#include "rawr/audio.h"
#include "rawr/error.h"
#include "rawr/ring.h"
#include "rawr/util.h"

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

typedef struct rawr_AudioDevicePriv {
    const PaDeviceInfo *deviceInfo;
} rawr_AudioDevicePriv;

typedef struct rawr_AudioDevice {
    rawr_AudioDevicePriv *priv;
    rawr_AudioDeviceId id;
    rawr_AudioDeviceProps props;
    rawr_AudioRateFlags rates;
} rawr_AudioDevice;

typedef struct rawr_AudioStreamPriv {
    PaStream *pa_stream;
    PaStreamParameters inParameters;
    PaStreamParameters outParameters;
    rawr_AudioSample *ringBufferDataTo;
    rawr_AudioSample *ringBufferDataFrom;
    rawr_RingBuffer rbToDevice;
    rawr_RingBuffer rbFromDevice;
} rawr_AudioStreamPriv;

typedef struct rawr_AudioStream {
    rawr_AudioStreamPriv *priv;
    rawr_AudioDevice *inDevice;
    rawr_AudioDevice *outDevice;
    rawr_AudioRate sampleRate;
    size_t sampleCapacity;
    rawr_AudioStreamState state;
    int channelCount;
    int sampleCount;
} rawr_AudioStream;

static rawr_AudioPrivate audio_priv = {0};

// --------------------------------------------------------------------------------------------------------------
int rawr_Audio_Setup(void)
{
    int errCode = 0;
    PaError err;

    if (audio_priv.initialized) return RAWR_OK;

    errCode = -1;
    RAWR_GUARD_CLEANUP(err = Pa_Initialize());
    audio_priv.initialized = 1;

    errCode = -2;
    RAWR_GUARD_CLEANUP((audio_priv.deviceCount = Pa_GetDeviceCount()) < 0);

    errCode = -3;
    RAWR_GUARD_NULL_CLEANUP(audio_priv.deviceList = MN_MEM_ACQUIRE(audio_priv.deviceCount * sizeof(*audio_priv.deviceList)));
    audio_priv.defaultInputId = Pa_GetDefaultInputDevice();
    audio_priv.defaultOutputId = Pa_GetDefaultOutputDevice();
    for (int d = 0; d < audio_priv.deviceCount; d++) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(d);
        rawr_AudioDevice *dev = audio_priv.deviceList + d;
        rawr_AudioDevicePriv *priv;

        errCode = -4;
        RAWR_GUARD_NULL_CLEANUP(priv = MN_MEM_ACQUIRE(sizeof(*priv)));
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
    RAWR_GUARD_CLEANUP(err = Pa_Terminate());

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
rawr_AudioDevicePriv *rawr_AudioDevice_Priv(const rawr_AudioDevice *const dev)
{
    RAWR_ASSERT(dev);
    return (rawr_AudioDevicePriv *)dev->priv;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_DefaultInput(void)
{
    RAWR_GUARD_CLEANUP(audio_priv.defaultInputId < 0 || audio_priv.defaultInputId >= audio_priv.deviceCount);
    return audio_priv.deviceList + audio_priv.defaultInputId;

cleanup:
    return NULL;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_DefaultOutput(void)
{
    RAWR_GUARD_CLEANUP(audio_priv.defaultOutputId < 0 || audio_priv.defaultOutputId >= audio_priv.deviceCount);
    return audio_priv.deviceList + audio_priv.defaultOutputId;

cleanup:
    return NULL;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDeviceId rawr_AudioDevice_Id(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->id;
}

// --------------------------------------------------------------------------------------------------------------
const char *rawr_AudioDevice_Name(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    rawr_AudioDevicePriv *priv = rawr_AudioDevice_Priv(dev);
    return priv->deviceInfo->name;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDeviceProps rawr_AudioDevice_Props(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->props;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioRateFlags rawr_AudioDevice_SampleRates(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return dev->rates;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioDevice_OutputChannels(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return rawr_AudioDevice_Priv(dev)->deviceInfo->maxOutputChannels;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioDevice_InputChannels(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return rawr_AudioDevice_Priv(dev)->deviceInfo->maxInputChannels;
}

// private ------------------------------------------------------------------------------------------------------
rawr_AudioStreamPriv *rawr_AudioStream_Priv(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);
    return (rawr_AudioStreamPriv *)stream->priv;
}

// private ------------------------------------------------------------------------------------------------------
int rawr_AudioStream_AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    rawr_AudioStream *stream = (rawr_AudioStream *)userData;
    RAWR_ASSERT(stream);
    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);

    size_t total = 96000;
    float pct;

    if (outputBuffer) {
        size_t avail = rawr_RingBuffer_GetReadAvailable(&priv->rbToDevice);
        pct = (float)avail / (float)stream->sampleCapacity;

        if (avail < framesPerBuffer) {
            mn_log_debug("silence");
            memset(outputBuffer, 0, framesPerBuffer * sizeof(rawr_AudioSample));
        } else {
            size_t read = rawr_RingBuffer_Read(&priv->rbToDevice, outputBuffer, framesPerBuffer);
        }
    }

    if (inputBuffer) {
        size_t avail = rawr_RingBuffer_GetWriteAvailable(&priv->rbFromDevice);
        
        pct = (float) avail / (float)stream->sampleCapacity;

        if (stream->state == rawr_AudioStreamState_Started) {
            if (pct < 0.2f) stream->state = rawr_AudioStreamState_Playing;
        }
        size_t wrote = rawr_RingBuffer_Write(&priv->rbFromDevice, inputBuffer, framesPerBuffer);
    }

    return paContinue;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Setup(rawr_AudioStream **out_stream, rawr_AudioRate sampleRate, int channelCount, int sampleCount)
{
    RAWR_ASSERT(out_stream);

    rawr_AudioStreamPriv *priv = NULL;
    uint32_t numBytes, numSamples;

    RAWR_GUARD_NULL(*out_stream = MN_MEM_ACQUIRE(sizeof(**out_stream)));
    (*out_stream)->outDevice = NULL;
    (*out_stream)->inDevice = NULL;
    (*out_stream)->sampleRate = sampleRate;
    (*out_stream)->channelCount = channelCount;
    (*out_stream)->sampleCount = sampleCount;
    (*out_stream)->state = rawr_AudioStreamState_Ready;
    numSamples = rawr_Util_NextPowerOf2((unsigned)((*out_stream)->sampleCount * 10));
    (*out_stream)->sampleCapacity = numSamples;
    
    RAWR_GUARD_NULL(priv = MN_MEM_ACQUIRE(sizeof(*priv)));
    memset(priv, 0, sizeof(*priv));
    (*out_stream)->priv = priv;

    numSamples = rawr_Util_NextPowerOf2((unsigned)((*out_stream)->sampleCount * 10));
    numBytes = numSamples * sizeof(rawr_AudioSample);
    priv->ringBufferDataTo = MN_MEM_ACQUIRE(numBytes);
    priv->ringBufferDataFrom = MN_MEM_ACQUIRE(numBytes);
    
    RAWR_GUARD_CLEANUP(rawr_RingBuffer_Initialize(&priv->rbToDevice, sizeof(rawr_AudioSample), numSamples, priv->ringBufferDataTo));
    RAWR_GUARD_CLEANUP(rawr_RingBuffer_Initialize(&priv->rbFromDevice, sizeof(rawr_AudioSample), numSamples, priv->ringBufferDataFrom));

    return rawr_Success;

cleanup:
    MN_MEM_RELEASE(priv->ringBufferDataTo);
    MN_MEM_RELEASE(priv->ringBufferDataFrom);
    MN_MEM_RELEASE(priv);
    MN_MEM_RELEASE(*out_stream);

    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_AudioStream_Cleanup(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);

    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);
    MN_MEM_RELEASE(priv->ringBufferDataTo);
    MN_MEM_RELEASE(priv->ringBufferDataFrom);
    MN_MEM_RELEASE(priv);
    MN_MEM_RELEASE(stream);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_AddDevice(rawr_AudioStream *stream, rawr_AudioDevice *dev)
{
    RAWR_ASSERT(stream);

    const int outChannels = rawr_AudioDevice_OutputChannels(dev);
    const int inChannels = rawr_AudioDevice_InputChannels(dev);

    RAWR_GUARD(outChannels && outChannels < stream->channelCount);
    RAWR_GUARD(inChannels && inChannels < stream->channelCount);

    if (outChannels) {
        RAWR_GUARD(stream->outDevice);
        RAWR_GUARD(outChannels < stream->channelCount);
        stream->outDevice = dev;
    }

    if (inChannels) {
        RAWR_GUARD(stream->inDevice);
        RAWR_GUARD(inChannels < stream->channelCount);
        stream->inDevice = dev;
    }

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Start(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);

    PaError err;
    int errCode;
    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);

    PaStreamParameters *pInParams = NULL;
    if (stream->inDevice) {
        pInParams = &priv->inParameters;
        pInParams->channelCount = stream->channelCount;
        pInParams->device = rawr_AudioDevice_Id(stream->inDevice);
        pInParams->sampleFormat = paInt16;
        pInParams->suggestedLatency = 0;
        pInParams->hostApiSpecificStreamInfo = NULL;
    }

    PaStreamParameters *pOutParams = NULL;
    if (stream->outDevice) {
        pOutParams = &priv->outParameters;
        pOutParams->channelCount = stream->channelCount;
        pOutParams->device = rawr_AudioDevice_Id(stream->outDevice);
        pOutParams->sampleFormat = paInt16;
        pOutParams->suggestedLatency = 0;
        pOutParams->hostApiSpecificStreamInfo = NULL;
    }

    errCode = -1;
    err = Pa_OpenStream(
        &priv->pa_stream,
        pInParams,
        pOutParams,
        (double)stream->sampleRate,
        stream->sampleCount,
        paClipOff,
        rawr_AudioStream_AudioCallback,
        stream
    );
    RAWR_GUARD_CLEANUP(err);

    errCode = -2;
    RAWR_GUARD_CLEANUP(Pa_StartStream(priv->pa_stream));

    stream->state = rawr_AudioStreamState_Playing;

    return rawr_Success;

cleanup:
    if (errCode <= -3) Pa_AbortStream(priv->pa_stream);
    if (errCode <= -2) Pa_CloseStream(priv->pa_stream);
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Stop(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);
    return Pa_StopStream(rawr_AudioStream_Priv(stream)->pa_stream);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Read(rawr_AudioStream *stream, void *buffer)
{
    RAWR_ASSERT(stream);

    rawr_RingBuffer *rb = &rawr_AudioStream_Priv(stream)->rbFromDevice;
    if (rawr_RingBuffer_GetReadAvailable(rb) < stream->sampleCount) {
        //memset(buffer, 0, stream->sampleCount * sizeof(rawr_AudioSample));
        return 0;
        //stream->sampleCount;
    }

    return rawr_RingBuffer_Read(rb, buffer, stream->sampleCount);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Write(rawr_AudioStream *stream, void *buffer)
{
    RAWR_ASSERT(stream);

    rawr_RingBuffer *rb = &rawr_AudioStream_Priv(stream)->rbToDevice;
    if (rawr_RingBuffer_GetWriteAvailable(rb) < stream->sampleCount) {
        return 0;
    }

    return rawr_RingBuffer_Write(rb, buffer, stream->sampleCount);
}
