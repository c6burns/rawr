#include "rawr/Audio.h"
#include "rawr/RingBuffer.h"
#include "rawr/Util.h"
#include "rawr/error.h"
#include "rawr/platform.h"

#include "mn/allocator.h"
#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"

#include <audioin.h>
#include <audioout.h>

#include <math.h>
#include <stdint.h>

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
    int placeholder;
} rawr_AudioDevicePriv;

typedef struct rawr_AudioDevice {
    rawr_AudioDevicePriv *priv;
    rawr_AudioDeviceId id;
    rawr_AudioDeviceProps props;
    rawr_AudioRateFlags rates;
} rawr_AudioDevice;

typedef struct rawr_AudioStreamPriv {
    SceUserServiceUserId userId;
    int32_t outHandle;
    int32_t inHandle;
} rawr_AudioStreamPriv;

typedef struct rawr_AudioStream {
    rawr_AudioStreamPriv *priv;

    rawr_AudioDevice *inDevice;
    rawr_AudioDevice *outDevice;

    rawr_AudioRate sampleRate;

    rawr_AudioSample *ringBufferDataTo;
    rawr_RingBuffer rbToDevice;

    rawr_AudioSample *ringBufferDataFrom;
    rawr_RingBuffer rbFromDevice;

    size_t sampleCapacity;
    int channelCount;
    int sampleCount;

    mn_atomic_t inputLevel;
    mn_atomic_t outputLevel;

    mn_thread_t audioThread;
} rawr_AudioStream;

// --------------------------------------------------------------------------------------------------------------
int rawr_Audio_Setup(void)
{
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Audio_Cleanup(void)
{
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_Get(rawr_AudioDeviceId id)
{
    RAWR_ASSERT(id > 0);
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
    return NULL;
}

// --------------------------------------------------------------------------------------------------------------
rawr_AudioDevice *rawr_AudioDevice_DefaultOutput(void)
{
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
    //rawr_AudioDevicePriv *priv = rawr_AudioDevice_Priv(dev);
    return "AudioDevice";
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
    return 1;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioDevice_InputChannels(rawr_AudioDevice *dev)
{
    RAWR_ASSERT(dev);
    return 1;
}

// private ------------------------------------------------------------------------------------------------------
rawr_AudioStreamPriv *rawr_AudioStream_Priv(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);
    return (rawr_AudioStreamPriv *)stream->priv;
}

// private ------------------------------------------------------------------------------------------------------
void rawr_AudioStream_AudioThread(void *arg)
{
    rawr_AudioStream *stream = (rawr_AudioStream *)arg;
    RAWR_ASSERT(stream);
    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);

    int ret;
    rawr_AudioSample inputSamples[SCE_AUDIO_IN_GRAIN_256] = {0};
    rawr_AudioSample outputSamples[SCE_AUDIO_IN_GRAIN_256] = {0};
    int32_t outvol[8];
    outvol[0] = outvol[1] = SCE_AUDIO_VOLUME_0dB;

    priv->outHandle = sceAudioOutOpen(
        SCE_USER_SERVICE_USER_ID_SYSTEM,
        SCE_AUDIO_OUT_PORT_TYPE_MAIN,
        0,
        SCE_AUDIO_IN_GRAIN_256,
        SCE_AUDIO_IN_FREQ_48K,
        SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);

    RAWR_GUARD_CLEANUP(priv->outHandle < 0);

    sceAudioOutSetVolume(priv->outHandle, (SCE_AUDIO_VOLUME_FLAG_FL_CH | SCE_AUDIO_VOLUME_FLAG_FR_CH), outvol);

    priv->inHandle = sceAudioInOpen(
        rawr_AudioStream_Priv(stream)->userId,
        SCE_AUDIO_IN_TYPE_GENERAL,
        0,
        SCE_AUDIO_IN_GRAIN_256,
        SCE_AUDIO_IN_FREQ_48K,
        SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);

    RAWR_GUARD_CLEANUP(priv->inHandle < 0);

    while (1) {
        size_t avail = rawr_RingBuffer_GetReadAvailable(&stream->rbToDevice);
        if (avail < SCE_AUDIO_IN_GRAIN_256) {
            /* emit silence */
            memset(outputSamples, 0, SCE_AUDIO_IN_GRAIN_256 * sizeof(rawr_AudioSample));
        } else {
            rawr_RingBuffer_Read(&stream->rbToDevice, outputSamples, SCE_AUDIO_IN_GRAIN_256);
        }

        RAWR_GUARD_CLEANUP((ret = sceAudioOutOutput(priv->outHandle, outputSamples)) < 0);
        RAWR_ASSERT(ret == SCE_AUDIO_IN_GRAIN_256);

        RAWR_GUARD_CLEANUP((ret = sceAudioInInput(priv->inHandle, inputSamples)) < 0);
        RAWR_ASSERT(ret == SCE_AUDIO_IN_GRAIN_256);

        double inputRms, inputDb, inputLevel, outputRms, outputDb, outputLevel;
        double weight = 1.0 / (double)SCE_AUDIO_IN_GRAIN_256;
        double inverse = 1.0 / 32767.0;

        inputRms = outputRms = 0.0;
        for (int i = 0; i < SCE_AUDIO_IN_GRAIN_256; i++) {
            inputRms += abs(*(inputSamples + i)) * inverse * weight;
            outputRms += abs(*(outputSamples + i)) * inverse * weight;
        }

        /* approx -230 to -10 is what you can expect to see here */
        inputDb = 20 * log(inputRms);
        outputDb = 20 * log(outputRms);

        /* drop the lowest range to make the level look stable */
        inputLevel = fmax(20.0, fmin(200.0, inputDb * -1.0)) - 20.0;
        inputLevel = 1.0 - (inputLevel / 180.0);

        outputLevel = fmax(20.0, fmin(200.0, outputDb * -1.0)) - 20.0;
        outputLevel = 1.0 - (outputLevel / 180.0);

        /* store in atomics for frontend retrival */
        mn_atomic_store(&stream->inputLevel, inputLevel * RAWR_AUDIOSTREAM_LEVEL_MULTIPLIER);
        mn_atomic_store(&stream->outputLevel, outputLevel * RAWR_AUDIOSTREAM_LEVEL_MULTIPLIER);

        rawr_RingBuffer_Write(&stream->rbFromDevice, inputSamples, SCE_AUDIO_IN_GRAIN_256);
    }

    return;

cleanup:
    mn_log_error("error");
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Setup(rawr_AudioStream **out_stream, rawr_AudioRate sampleRate, int channelCount, int sampleCount)
{
    RAWR_ASSERT(out_stream);

    int ret;
    rawr_AudioStreamPriv *priv = NULL;
    uint32_t numBytes, numSamples;

    numSamples = rawr_Util_NextPowerOf2((unsigned)(sampleCount * 10));
    numBytes = numSamples * sizeof(rawr_AudioSample);

    RAWR_GUARD_NULL(*out_stream = MN_MEM_ACQUIRE(sizeof(**out_stream)));
    rawr_AudioStream *stream = *out_stream;
    stream->outDevice = NULL;
    stream->inDevice = NULL;
    stream->sampleRate = sampleRate;
    stream->channelCount = channelCount;
    stream->sampleCount = sampleCount;
    stream->sampleCapacity = numSamples;
    stream->ringBufferDataTo = MN_MEM_ACQUIRE(numBytes);
    stream->ringBufferDataFrom = MN_MEM_ACQUIRE(numBytes);

    RAWR_GUARD_CLEANUP(rawr_RingBuffer_Initialize(&stream->rbToDevice, sizeof(rawr_AudioSample), numSamples, stream->ringBufferDataTo));
    RAWR_GUARD_CLEANUP(rawr_RingBuffer_Initialize(&stream->rbFromDevice, sizeof(rawr_AudioSample), numSamples, stream->ringBufferDataFrom));

    RAWR_GUARD_NULL(priv = MN_MEM_ACQUIRE(sizeof(*priv)));
    memset(priv, 0, sizeof(*priv));
    stream->priv = priv;

    mn_atomic_store(&stream->inputLevel, 0);
    mn_atomic_store(&stream->outputLevel, 0);

    mn_thread_setup(&stream->audioThread);

    RAWR_GUARD_CLEANUP((ret = sceUserServiceInitialize(NULL)) != SCE_OK);

    /* audio output initialize */
    RAWR_GUARD_CLEANUP((ret = sceAudioOutInit()) && (ret != SCE_AUDIO_OUT_ERROR_ALREADY_INIT));

    RAWR_GUARD_CLEANUP((ret = sceUserServiceGetInitialUser(&priv->userId)) < 0);

    return rawr_Success;

cleanup:
    MN_MEM_RELEASE(priv);
    MN_MEM_RELEASE(stream->ringBufferDataTo);
    MN_MEM_RELEASE(stream->ringBufferDataFrom);
    MN_MEM_RELEASE(stream);

    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_AudioStream_Cleanup(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);

    sceUserServiceTerminate();

    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);
    MN_MEM_RELEASE(priv);
    MN_MEM_RELEASE(stream->ringBufferDataTo);
    MN_MEM_RELEASE(stream->ringBufferDataFrom);
    MN_MEM_RELEASE(stream);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_AddDevice(rawr_AudioStream *stream, rawr_AudioDevice *dev)
{
    RAWR_ASSERT(stream);
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Start(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);
    //rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);

    RAWR_GUARD_CLEANUP(mn_thread_launch(&stream->audioThread, rawr_AudioStream_AudioThread, stream));

    return rawr_Success;

cleanup:
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Stop(rawr_AudioStream *stream)
{
    RAWR_ASSERT(stream);
    rawr_AudioStreamPriv *priv = rawr_AudioStream_Priv(stream);

    int ret;

    sceAudioInInput(priv->inHandle, NULL);

    RAWR_GUARD_CLEANUP((ret = sceAudioInClose(priv->inHandle)) < 0);

    sceAudioOutOutput(priv->outHandle, NULL);

    RAWR_GUARD_CLEANUP((ret = sceAudioOutClose(priv->outHandle)) < 0);

    return rawr_Success;

cleanup:
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Read(rawr_AudioStream *stream, void *buffer)
{
    RAWR_ASSERT(stream);

    rawr_RingBuffer *rb = &stream->rbFromDevice;
    if (rawr_RingBuffer_GetReadAvailable(rb) < stream->sampleCount) {
        return 0;
    }

    return rawr_RingBuffer_Read(rb, buffer, stream->sampleCount);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_AudioStream_Write(rawr_AudioStream *stream, void *buffer)
{
    RAWR_ASSERT(stream);

    rawr_RingBuffer *rb = &stream->rbToDevice;
    if (rawr_RingBuffer_GetWriteAvailable(rb) < stream->sampleCount) {
        return 0;
    }

    return rawr_RingBuffer_Write(rb, buffer, stream->sampleCount);
}

// --------------------------------------------------------------------------------------------------------------
double rawr_AudioStream_InputLevel(rawr_AudioStream *stream)
{
    return (double)mn_atomic_load(&stream->inputLevel) / RAWR_AUDIOSTREAM_LEVEL_MULTIPLIER;
}

// --------------------------------------------------------------------------------------------------------------
double rawr_AudioStream_OutputLevel(rawr_AudioStream *stream)
{
    return (double)mn_atomic_load(&stream->outputLevel) / RAWR_AUDIOSTREAM_LEVEL_MULTIPLIER;
}
