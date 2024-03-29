#ifndef RAWR_OPUS_H
#define RAWR_OPUS_H

#include "rawr/Audio.h"
#include "rawr/Platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAWR_CODEC_OUTPUT_BYTES_MAX 1500
#define RAWR_CODEC_OUTPUT_SAMPLES_MAX (RAWR_CODEC_OUTPUT_BYTES_MAX / sizeof(rawr_AudioSample))

#define RAWR_CODEC_INPUT_SAMPLES_MAX 5760
#define RAWR_CODEC_INPUT_BYTES_MAX (RAWR_CODEC_INPUT_SAMPLES_MAX * sizeof(rawr_AudioSample))

typedef struct rawr_Codec rawr_Codec;

typedef enum rawr_CodecType {
    rawr_CodecType_Encoder,
    rawr_CodecType_Decoder,
} rawr_CodecType;

typedef enum rawr_CodecRate {
    rawr_CodecRate_8k = 8000,
    rawr_CodecRate_16k = 16000,
    rawr_CodecRate_24k = 24000,
    rawr_CodecRate_48k = 48000,
} rawr_CodecRate;

typedef enum rawr_CodecTiming {
    rawr_CodecTiming_5ms = 5,
    rawr_CodecTiming_10ms = 10,
    rawr_CodecTiming_20ms = 20,
    rawr_CodecTiming_40ms = 40,
    rawr_CodecTiming_60ms = 60,
    rawr_CodecTiming_80ms = 80,
    rawr_CodecTiming_100ms = 100,
    rawr_CodecTiming_120ms = 120,
} rawr_CodecTiming;

typedef struct rawr_CodecConfig {
    int application;
    int bitrate;
    int force_channel;
    int vbr;
    int vbr_constraint;
    int complexity;
    int max_bw;
    int sig;
    int inband_fec;
    int pkt_loss;
    int lsb_depth;
    int pred_disabled;
    int dtx;
} rawr_CodecConfig;

RAWR_API int RAWR_CALL rawr_Codec_Setup(rawr_Codec **out_codec, rawr_CodecType type, rawr_CodecRate rate, rawr_CodecTiming time);
RAWR_API void RAWR_CALL rawr_Codec_Cleanup(rawr_Codec *codec);

RAWR_API int RAWR_CALL rawr_Codec_Encode(rawr_Codec *codec, void *inBuffer, void *outBuffer);
RAWR_API int RAWR_CALL rawr_Codec_Decode(rawr_Codec *codec, void *inBuffer, int byteLen, void *outBuffer);

RAWR_API int RAWR_CALL rawr_Codec_FrameSize(rawr_CodecRate sampleRate, rawr_CodecTiming timing);
RAWR_API int RAWR_CALL rawr_Codec_FrameSizeCode(rawr_CodecRate sampleRate, int sampleCount);

#ifdef __cplusplus
}
#endif

#endif
