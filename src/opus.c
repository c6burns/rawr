#include "rawr/opus.h"
#include "rawr/error.h"

#include "opus.h"

#include "mn/allocator.h"
#include "mn/error.h"
#include "mn/log.h"


static rawr_CodecConfig codecConfig = {
    .application = OPUS_APPLICATION_VOIP,
    .bitrate = OPUS_AUTO,
    .force_channel = 1,
    .vbr = 1,
    .vbr_constraint = 1,
    .complexity = 5,
    .max_bw = OPUS_BANDWIDTH_FULLBAND,
    .sig = OPUS_SIGNAL_VOICE,
    .inband_fec = 0,
    .pkt_loss = 5,
    .lsb_depth = 8,
    .pred_disabled = 0,
    .dtx = 1,
};

typedef struct rawr_CodecPriv {
    OpusEncoder *opus_enc;
    OpusDecoder *opus_dec;
} rawr_CodecPriv;

typedef struct rawr_Codec {
    rawr_CodecPriv *priv;
    rawr_CodecConfig config;
    rawr_CodecType type;
    rawr_CodecRate sampleRate;
    rawr_CodecTiming timing;
    int frameSize;
    int frameSizeCode;
} rawr_Codec;

// private ------------------------------------------------------------------------------------------------------
rawr_CodecPriv *rawr_Codec_Priv(rawr_Codec *codec)
{
    RAWR_ASSERT(codec);
    return codec->priv;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_Setup(rawr_Codec **out_codec, rawr_CodecType type, rawr_CodecRate sampleRate, rawr_CodecTiming timing)
{
    RAWR_ASSERT(out_codec);

    int err;
    rawr_Codec *codec = NULL;
    rawr_CodecPriv *priv = NULL;

    RAWR_GUARD_NULL(*out_codec = MN_MEM_ACQUIRE(sizeof(**out_codec)));
    *out_codec = codec;
    codec->frameSize = rawr_Codec_FrameSize(sampleRate, timing);
    codec->frameSizeCode = rawr_Codec_FrameSize(sampleRate, timing);
    
    RAWR_GUARD_NULL(priv = MN_MEM_ACQUIRE(sizeof(*priv)));
    (*out_codec)->priv = priv;
    priv->opus_dec = NULL;
    priv->opus_enc = NULL;

    if (codec->type == rawr_CodecType_Encoder) {
        priv->opus_enc = opus_encoder_create(sampleRate, codecConfig.force_channel, codecConfig.application, &err);
        RAWR_GUARD_CLEANUP(err != OPUS_OK || priv->opus_enc == NULL);

        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_BITRATE(codecConfig.bitrate)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_FORCE_CHANNELS(codecConfig.force_channel)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_VBR(codecConfig.vbr)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_VBR_CONSTRAINT(codecConfig.vbr_constraint)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_COMPLEXITY(codecConfig.complexity)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_MAX_BANDWIDTH(codecConfig.max_bw)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_SIGNAL(codecConfig.sig)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_INBAND_FEC(codecConfig.inband_fec)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_PACKET_LOSS_PERC(codecConfig.pkt_loss)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_LSB_DEPTH(codecConfig.lsb_depth)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_PREDICTION_DISABLED(codecConfig.pred_disabled)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_DTX(codecConfig.dtx)) != OPUS_OK);
        RAWR_GUARD_CLEANUP(opus_encoder_ctl(priv->opus_enc, OPUS_SET_EXPERT_FRAME_DURATION(codec->frameSizeCode) != OPUS_OK));

        return rawr_Success;
    } else if (codec->type == rawr_CodecType_Decoder) {
        priv->opus_dec = opus_decoder_create(sampleRate, codecConfig.force_channel, &err);
        RAWR_GUARD_CLEANUP(err != OPUS_OK || priv->opus_dec == NULL);

        return rawr_Success;
    }

cleanup:
    MN_MEM_RELEASE(priv);
    MN_MEM_RELEASE(codec);
    return rawr_Error;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_Cleanup(rawr_Codec *codec)
{
    RAWR_ASSERT(codec);
    MN_MEM_RELEASE(codec->priv);
    MN_MEM_RELEASE(codec);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_Encode(rawr_Codec *codec, void *inBuffer, void *outBuffer)
{
    RAWR_ASSERT(codec);

    rawr_CodecPriv *priv = rawr_Codec_Priv(codec);
    RAWR_ASSERT(priv && priv->opus_enc);

    int out_bytes = opus_encode(priv->opus_enc, inBuffer, codec->frameSize, outBuffer, RAWR_CODEC_OUTPUT_BYTES_MAX);
    RAWR_ASSERT(0 <= out_bytes && out_bytes <= RAWR_CODEC_OUTPUT_BYTES_MAX);

    return out_bytes;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_Decode(rawr_Codec *codec, void *inBuffer, int byteLen, void *outBuffer)
{
    RAWR_ASSERT(codec);

    rawr_CodecPriv *priv = rawr_Codec_Priv(codec);
    RAWR_ASSERT(priv && priv->opus_enc);

    int out_samples = opus_decode(priv->opus_dec, inBuffer, byteLen, outBuffer, RAWR_CODEC_INPUT_BYTES_MAX, codecConfig.inband_fec);
    RAWR_ASSERT(out_samples == codec->frameSize);

    return out_samples;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_FrameSize(rawr_CodecRate sampleRate, rawr_CodecTiming timing)
{
    return timing * 2 * (sampleRate / 2000);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Codec_FrameSizeCode(rawr_CodecRate sampleRate, int sampleCount)
{
    int code = OPUS_FRAMESIZE_ARG;

    if (sampleCount == sampleRate / 400)
        code = OPUS_FRAMESIZE_2_5_MS;
    else if (sampleCount == sampleRate / 200)
        code = OPUS_FRAMESIZE_5_MS;
    else if (sampleCount == sampleRate / 100)
        code = OPUS_FRAMESIZE_10_MS;
    else if (sampleCount == sampleRate / 50)
        code = OPUS_FRAMESIZE_20_MS;
    else if (sampleCount == sampleRate / 25)
        code = OPUS_FRAMESIZE_40_MS;
    else if (sampleCount == 3 * sampleRate / 50)
        code = OPUS_FRAMESIZE_60_MS;
    else if (sampleCount == 4 * sampleRate / 50)
        code = OPUS_FRAMESIZE_80_MS;
    else if (sampleCount == 5 * sampleRate / 50)
        code = OPUS_FRAMESIZE_100_MS;
    else if (sampleCount == 6 * sampleRate / 50)
        code = OPUS_FRAMESIZE_120_MS;

    return code;
}
