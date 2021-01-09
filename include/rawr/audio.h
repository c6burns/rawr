#ifndef RAWR_AUDIO_H
#define RAWR_AUDIO_H

#define RAWR_AUDIOSTREAM_SAMPLECOUNT_MAX

typedef short rawr_AudioSample;

typedef int rawr_AudioDeviceId;

typedef enum rawr_AudioRate {
    rawr_AudioRate_8000 = 8000,
    rawr_AudioRate_9600 = 9600,
    rawr_AudioRate_11025 = 11025,
    rawr_AudioRate_12000 = 12000,
    rawr_AudioRate_16000 = 16000,
    rawr_AudioRate_22050 = 22050,
    rawr_AudioRate_24000 = 24000,
    rawr_AudioRate_32000 = 32000,
    rawr_AudioRate_44100 = 44100,
    rawr_AudioRate_48000 = 48000,
    rawr_AudioRate_88200 = 88200,
    rawr_AudioRate_96000 = 96000,
    rawr_AudioRate_192000 = 192000,
} rawr_AudioRate;

typedef enum rawr_AudioRateFlags {
    rawr_AudioRateFlags_None,
    rawr_AudioRateFlags_8000 = 1 << 0,
    rawr_AudioRateFlags_9600 = 1 << 1,
    rawr_AudioRateFlags_11025 = 1 << 2,
    rawr_AudioRateFlags_12000 = 1 << 3,
    rawr_AudioRateFlags_16000 = 1 << 4,
    rawr_AudioRateFlags_22050 = 1 << 5,
    rawr_AudioRateFlags_24000 = 1 << 6,
    rawr_AudioRateFlags_32000 = 1 << 7,
    rawr_AudioRateFlags_44100 = 1 << 8,
    rawr_AudioRateFlags_48000 = 1 << 9,
    rawr_AudioRateFlags_88200 = 1 << 10,
    rawr_AudioRateFlags_96000 = 1 << 11,
    rawr_AudioRateFlags_192000 = 1 << 12,
} rawr_AudioRateFlags;

typedef enum rawr_AudioDeviceProps {
    rawr_AudioDeviceProps_None,
    rawr_AudioDeviceProps_Default = 1 << 0,
    rawr_AudioDeviceProps_Output = 1 << 1,
    rawr_AudioDeviceProps_Input = 1 << 2,
    rawr_AudioDeviceProps_OutputStereo = 1 << 3,
    rawr_AudioDeviceProps_InputStereo = 1 << 4,
} rawr_AudioDeviceProps;

typedef enum rawr_AudioStreamState {
    rawr_AudioStreamState_New,
    rawr_AudioStreamState_Ready,
    rawr_AudioStreamState_Started,
    rawr_AudioStreamState_Playing,
    rawr_AudioStreamState_Stopped,
} rawr_AudioStreamState;

typedef struct rawr_AudioDevice rawr_AudioDevice;

typedef struct rawr_AudioStream rawr_AudioStream;

int rawr_Audio_Setup(void);
int rawr_Audio_Cleanup(void);

rawr_AudioDevice *rawr_AudioDevice_Get(rawr_AudioDeviceId id);
rawr_AudioDevice *rawr_AudioDevice_DefaultInput(void);
rawr_AudioDevice *rawr_AudioDevice_DefaultOutput(void);

rawr_AudioDeviceId rawr_AudioDevice_Id(rawr_AudioDevice *dev);
const char *rawr_AudioDevice_Name(rawr_AudioDevice *dev);
rawr_AudioDeviceProps rawr_AudioDevice_Props(rawr_AudioDevice *dev);
rawr_AudioRateFlags rawr_AudioDevice_SampleRates(rawr_AudioDevice *dev);
int rawr_AudioDevice_OutputChannels(rawr_AudioDevice *dev);
int rawr_AudioDevice_InputChannels(rawr_AudioDevice *dev);

int rawr_AudioStream_Setup(rawr_AudioStream **out_stream, rawr_AudioRate sampleRate, int channelCount, int sampleCount);
void rawr_AudioStream_Cleanup(rawr_AudioStream *stream);
int rawr_AudioStream_AddDevice(rawr_AudioStream *stream, rawr_AudioDevice *dev);
int rawr_AudioStream_Start(rawr_AudioStream *stream);
int rawr_AudioStream_Stop(rawr_AudioStream *stream);
int rawr_AudioStream_Read(rawr_AudioStream *stream, void *buffer);
int rawr_AudioStream_Write(rawr_AudioStream *stream, void *buffer);

#endif
