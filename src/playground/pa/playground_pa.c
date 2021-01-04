#include "rawr/audio.h"
#include "rawr/error.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "portaudio.h"

#include "opus.h"
#include <stdlib.h>

#define MAX_PACKET (1500)
#define SAMPLES (48000 * 30)
#define SSAMPLES (SAMPLES / 3)
#define MAX_FRAME_SAMP (5760)
#define PI (3.141592653589793238462643f)

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (960)
#define NUM_SECONDS (10)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG (0)

/* Select sample format. */
#if 0
#    define PA_SAMPLE_TYPE paFloat32
#    define SAMPLE_SIZE (4)
#    define SAMPLE_SILENCE (0.0f)
#    define PRINTF_S_FORMAT "%.8f"
#elif 1
#    define PA_SAMPLE_TYPE paInt16
#    define SAMPLE_SIZE (2)
#    define SAMPLE_SILENCE (0)
#    define PRINTF_S_FORMAT "%d"
#elif 0
#    define PA_SAMPLE_TYPE paInt24
#    define SAMPLE_SIZE (3)
#    define SAMPLE_SILENCE (0)
#    define PRINTF_S_FORMAT "%d"
#elif 0
#    define PA_SAMPLE_TYPE paInt8
#    define SAMPLE_SIZE (1)
#    define SAMPLE_SILENCE (0)
#    define PRINTF_S_FORMAT "%d"
#else
#    define PA_SAMPLE_TYPE paUInt8
#    define SAMPLE_SIZE (1)
#    define SAMPLE_SILENCE (128)
#    define PRINTF_S_FORMAT "%d"
#endif



int main(void)
{
    char *sampleBlock = NULL;
    int i;
    int numBytes;
    int numChannels = 1;

    OpusEncoder *enc;
    OpusDecoder *dec;
    int j;
    int err;

    int sampling_rate = 48000;
    int num_channels = 1;
    int application = OPUS_APPLICATION_VOIP;

    int samp_count = 0;
    opus_int16 *inbuf = NULL;
    unsigned char packet[MAX_PACKET + 257];
    int len;
    opus_int16 *outbuf = NULL;
    int out_samples;
    int ret = 0;

    int bitrate = OPUS_AUTO;
    int force_channel = 1;
    int vbr = 1;
    int vbr_constraint = 1;
    int complexity = 5;
    int max_bw = OPUS_BANDWIDTH_FULLBAND;
    int sig = OPUS_SIGNAL_VOICE;
    int inband_fec = 0;
    int pkt_loss = 1;
    int lsb_depth = 8;
    int pred_disabled = 0;
    int dtx = 1;
    int frame_size_ms_x2 = 40;
    int frame_size = frame_size_ms_x2 * sampling_rate / 2000;
    int frame_size_enum = OPUS_FRAMESIZE_20_MS;

    mn_log_trace("testing portaudio and opus ...");

    rawr_Audio_Setup();

    rawr_AudioDevice *inDevice = rawr_AudioDevice_DefaultInput();
    rawr_AudioDevice *outDevice = rawr_AudioDevice_DefaultOutput();

    mn_log_info(" input: %d - %s", rawr_AudioDevice_Id(inDevice), rawr_AudioDevice_Name(inDevice));
    mn_log_info("output: %d - %s", rawr_AudioDevice_Id(outDevice), rawr_AudioDevice_Name(outDevice));

    rawr_AudioStream *stream = NULL;
    if (rawr_AudioStream_Setup(&stream, rawr_AudioRate_48000, 1, frame_size)) {
        mn_log_error("rawr_AudioStream_Setup failed");
    }

    if (rawr_AudioStream_AddDevice(stream, inDevice)) {
        mn_log_error("rawr_AudioStream_AddDevice failed on input");
    }

    if (rawr_AudioStream_AddDevice(stream, outDevice)) {
        mn_log_error("rawr_AudioStream_AddDevice failed on output");
    }

    numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
    RAWR_GUARD_NULL_CLEANUP(sampleBlock = MN_MEM_ACQUIRE(numBytes));
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);

    RAWR_GUARD_NULL_CLEANUP(inbuf = MN_MEM_ACQUIRE(sizeof(*inbuf) * frame_size));
    RAWR_GUARD_NULL_CLEANUP(outbuf = MN_MEM_ACQUIRE(sizeof(*outbuf) * frame_size));

    dec = opus_decoder_create(sampling_rate, num_channels, &err);
    RAWR_GUARD_CLEANUP(err != OPUS_OK || dec == NULL);

    enc = opus_encoder_create(sampling_rate, num_channels, application, &err);
    RAWR_GUARD_CLEANUP(err != OPUS_OK || enc == NULL);

    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(force_channel)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_VBR(vbr)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(vbr_constraint)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_MAX_BANDWIDTH(max_bw)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_SIGNAL(sig)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(inband_fec)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(pkt_loss)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(lsb_depth)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_PREDICTION_DISABLED(pred_disabled)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_DTX(dtx)) != OPUS_OK);
    RAWR_GUARD_CLEANUP(opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(frame_size_enum)) != OPUS_OK);

    mn_log_info("encoder_settings: %d kHz, %d ch, application: %d, "
                "%d bps, force ch: %d, vbr: %d, vbr constraint: %d, complexity: %d, "
                "max bw: %d, signal: %d, inband fec: %d, pkt loss: %d%%, lsb depth: %d, "
                "pred disabled: %d, dtx: %d, (%d/2) ms\n",
        sampling_rate / 1000,
        num_channels,
        application,
        bitrate,
        force_channel,
        vbr,
        vbr_constraint,
        complexity,
        max_bw,
        sig,
        inband_fec,
        pkt_loss,
        lsb_depth,
        pred_disabled,
        dtx,
        frame_size_ms_x2);


    RAWR_GUARD_CLEANUP(rawr_AudioStream_Start(stream));
    mn_log_trace("Talk (or whatever) for %d seconds.", NUM_SECONDS);

    for (i = 0; i < (NUM_SECONDS * SAMPLE_RATE) / FRAMES_PER_BUFFER; ++i) {
        RAWR_GUARD_CLEANUP(rawr_AudioStream_Read(stream, sampleBlock));

        /* encode data here, and then ... decode for sanity check */
        samp_count = 0;
        len = opus_encode(enc, (opus_int16 *)sampleBlock, frame_size, packet, MAX_PACKET);
        RAWR_GUARD_CLEANUP(len < 0 || len > MAX_PACKET);

        /* decode data here for sanity check */
        out_samples = opus_decode(dec, packet, len, outbuf, MAX_FRAME_SAMP, 0);
        RAWR_GUARD_CLEANUP(out_samples != frame_size);
        samp_count += frame_size;

        RAWR_GUARD_CLEANUP(rawr_AudioStream_Write(stream, sampleBlock));
    }
    mn_log_trace("Wire off.");

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Stop(stream));

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Cleanup(stream));

    MN_MEM_RELEASE(inbuf);
    MN_MEM_RELEASE(outbuf);
    MN_MEM_RELEASE(sampleBlock);

    RAWR_GUARD_CLEANUP(rawr_Audio_Cleanup());

    return rawr_Success;

cleanup:

    mn_log_error("FAILED!");

    return rawr_Error;
}
