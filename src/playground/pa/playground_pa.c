#include "rawr/pa.h"

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

int get_frame_size_enum(int frame_size, int sampling_rate)
{
    int frame_size_enum;

    if (frame_size == sampling_rate / 400)
        frame_size_enum = OPUS_FRAMESIZE_2_5_MS;
    else if (frame_size == sampling_rate / 200)
        frame_size_enum = OPUS_FRAMESIZE_5_MS;
    else if (frame_size == sampling_rate / 100)
        frame_size_enum = OPUS_FRAMESIZE_10_MS;
    else if (frame_size == sampling_rate / 50)
        frame_size_enum = OPUS_FRAMESIZE_20_MS;
    else if (frame_size == sampling_rate / 25)
        frame_size_enum = OPUS_FRAMESIZE_40_MS;
    else if (frame_size == 3 * sampling_rate / 50)
        frame_size_enum = OPUS_FRAMESIZE_60_MS;
    else if (frame_size == 4 * sampling_rate / 50)
        frame_size_enum = OPUS_FRAMESIZE_80_MS;
    else if (frame_size == 5 * sampling_rate / 50)
        frame_size_enum = OPUS_FRAMESIZE_100_MS;
    else if (frame_size == 6 * sampling_rate / 50)
        frame_size_enum = OPUS_FRAMESIZE_120_MS;

    return frame_size_enum;
}

int main(void)
{
    mn_log_trace("testing portaudio ...");

    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream = NULL;
    PaError err;
    const PaDeviceInfo *inputInfo;
    const PaDeviceInfo *outputInfo;
    char *sampleBlock = NULL;
    int i;
    int numBytes;
    int numChannels;

    OpusEncoder *enc;
    OpusDecoder *dec;
    int j;

    int sampling_rate = 48000;
    int num_channels = 1;
    int application = OPUS_APPLICATION_VOIP;

    int samp_count = 0;
    opus_int16 *inbuf;
    unsigned char packet[MAX_PACKET + 257];
    int len;
    opus_int16 *outbuf;
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
    int frame_size_enum = get_frame_size_enum(frame_size, sampling_rate);
    force_channel = 1;

    /* Generate input data */
    inbuf = (opus_int16 *)malloc(sizeof(*inbuf) * SSAMPLES);
    //generate_music(inbuf, SSAMPLES / 2);

    /* Allocate memory for output data */
    outbuf = (opus_int16 *)malloc(sizeof(*outbuf) * MAX_FRAME_SAMP * 3);

    mn_log_trace("patest_read_write_wire.c");
    mn_log_trace("sizeof(int) = %lu", sizeof(int));
    mn_log_trace("sizeof(long) = %lu", sizeof(long));

    err = Pa_Initialize();
    if (err != paNoError) goto error2;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    mn_log_trace("Input device # %d.", inputParameters.device);
    inputInfo = Pa_GetDeviceInfo(inputParameters.device);
    mn_log_trace("    Name: %s", inputInfo->name);
    mn_log_trace("      LL: %g s", inputInfo->defaultLowInputLatency);
    mn_log_trace("      HL: %g s", inputInfo->defaultHighInputLatency);

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    mn_log_trace("Output device # %d.", outputParameters.device);
    outputInfo = Pa_GetDeviceInfo(outputParameters.device);
    mn_log_trace("   Name: %s", outputInfo->name);
    mn_log_trace("     LL: %g s", outputInfo->defaultLowOutputLatency);
    mn_log_trace("     HL: %g s", outputInfo->defaultHighOutputLatency);

    numChannels = inputInfo->maxInputChannels < outputInfo->maxOutputChannels
                      ? inputInfo->maxInputChannels
                      : outputInfo->maxOutputChannels;
    numChannels = 1;
    mn_log_trace("Num channels = %d.", numChannels);

    inputParameters.channelCount = numChannels;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = inputInfo->defaultHighInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.channelCount = numChannels;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = outputInfo->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* -- setup -- */

    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        SAMPLE_RATE,
        frame_size,
        paClipOff, /* we won't output out of range samples so don't bother clipping them */
        NULL,      /* no callback, use blocking API */
        NULL);     /* no callback, so no callback userData */
    if (err != paNoError) goto error2;

    numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
    sampleBlock = (char *)malloc(numBytes);
    if (sampleBlock == NULL) {
        mn_log_trace("Could not allocate record array.");
        goto error1;
    }
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);


    dec = opus_decoder_create(sampling_rate, num_channels, &err);
    if (err != OPUS_OK || dec == NULL) return -1;

    enc = opus_encoder_create(sampling_rate, num_channels, application, &err);
    if (err != OPUS_OK || enc == NULL) return -1;

    if (opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(force_channel)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_VBR(vbr)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(vbr_constraint)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_MAX_BANDWIDTH(max_bw)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_SIGNAL(sig)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(inband_fec)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(pkt_loss)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(lsb_depth)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_PREDICTION_DISABLED(pred_disabled)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_DTX(dtx)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(frame_size_enum)) != OPUS_OK) goto cleanup;

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


    err = Pa_StartStream(stream);
    if (err != paNoError) goto error1;
    mn_log_trace("Wire on. Will run %d seconds.", NUM_SECONDS);

     // You may get underruns or overruns if the output is not primed by PortAudio.
    for (int primer = 0; primer < 4; primer++) {
        err = Pa_WriteStream(stream, sampleBlock, FRAMES_PER_BUFFER);
        if (err) goto xrun;
    }

    for (i = 0; i < (NUM_SECONDS * SAMPLE_RATE) / FRAMES_PER_BUFFER; ++i) {
        err = Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);
        if (err) goto xrun;

        /* Encode data, then decode for sanity check */
        samp_count = 0;
        len = opus_encode(enc, (opus_int16 *)sampleBlock, frame_size, packet, MAX_PACKET);
        if (len < 0 || len > MAX_PACKET) {
            mn_log_error("opus_encode() returned %d", len);
            ret = -1;
            break;
        }

        out_samples = opus_decode(dec, packet, len, outbuf, MAX_FRAME_SAMP, 0);
        if (out_samples != frame_size) {
            mn_log_error("opus_decode() returned %d", out_samples);
            ret = -1;
            break;
        }
        samp_count += frame_size;

         // You may get underruns or overruns if the output is not primed by PortAudio.
        err = Pa_WriteStream(stream, outbuf, FRAMES_PER_BUFFER);
        if (err) goto xrun;
    }
    mn_log_trace("Wire off.");

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error1;

    free(inbuf);
    free(outbuf);

    free(sampleBlock);

    Pa_Terminate();
    return 0;

cleanup:

    mn_log_error("FAIL!");

    if (inbuf) free(inbuf);
    if (outbuf) free(outbuf);

    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);

xrun:
    mn_log_trace("err = %d", err);
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }
    free(sampleBlock);
    Pa_Terminate();
    if (err & paInputOverflow)
        mn_log_error("Input Overflow.");
    if (err & paOutputUnderflow)
        mn_log_error("Output Underflow.");
    return -2;
error1:
    free(sampleBlock);
error2:
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate();
    mn_log_error("An error occurred while using the portaudio stream");
    mn_log_error("Error number: %d", err);
    mn_log_error("Error message: %s", Pa_GetErrorText(err));
    return -1;
}
