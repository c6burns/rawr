#include "rawr/Audio.h"
#include "rawr/Error.h"
#include "rawr/Codec.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

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

void threaded_decode_fn(rawr_AudioStream *stream)
{

}

void threaded_encode_fn(rawr_AudioStream *stream)
{

}

int main(void)
{
    char *sampleBlock = NULL;
    int numBytes;
    int numChannels = 1;

    int sampling_rate = 48000;

    int sampleCount = 0;
    opus_int16 *inbuf = NULL;
    unsigned char packet[MAX_PACKET + 257];
    int len;
    opus_int16 *outbuf = NULL;

    int frame_size_ms_x2 = 40;
    int frame_size = frame_size_ms_x2 * sampling_rate / 2000;

    rawr_Codec *decoder, *encoder;

    mn_thread_t thread_decode = {0};
    mn_thread_t thread_encode = {0};

    mn_log_trace("testing threaded portaudio and opus ...");

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

    RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&encoder, rawr_CodecType_Encoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));
    RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&decoder, rawr_CodecType_Decoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Start(stream));
    mn_log_trace("Talk (or whatever) and hear it back", NUM_SECONDS);

    mn_thread_setup(&thread_decode);
    //mn_thread_launch(&thread_decode, threaded_decode_fn, stream);

    mn_thread_setup(&thread_encode);
    //mn_thread_launch(&thread_encode, threaded_encode_fn, stream);

    for (;;) {
        sampleCount = 0;
        while ((sampleCount = rawr_AudioStream_Read(stream, sampleBlock)) == 0) {}
        RAWR_GUARD_CLEANUP(sampleCount < 0);

        RAWR_GUARD_CLEANUP((len = rawr_Codec_Encode(encoder, sampleBlock, packet)) < 0);

        /* decode data here for sanity check */
        RAWR_GUARD_CLEANUP(rawr_Codec_Decode(decoder, packet, len, outbuf) < 0);

        sampleCount = 0;
        while ((sampleCount = rawr_AudioStream_Write(stream, outbuf)) == 0) {}
        RAWR_GUARD_CLEANUP(sampleCount < 0);
    }
    mn_log_trace("Wire off.");

    rawr_Codec_Cleanup(encoder);
    rawr_Codec_Cleanup(decoder);

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Stop(stream));

    rawr_AudioStream_Cleanup(stream);

    MN_MEM_RELEASE(inbuf);
    MN_MEM_RELEASE(outbuf);
    MN_MEM_RELEASE(sampleBlock);

    RAWR_GUARD_CLEANUP(rawr_Audio_Cleanup());

    return rawr_Success;

cleanup:

    mn_log_error("FAILED!");

    return rawr_Error;
}
