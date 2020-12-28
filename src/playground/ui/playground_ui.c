#include "rawr/pa.h"

#include "rawr/term.h"
#include "rawr/pa.h"
#include "mn/thread.h"
#include "mn/time.h"

#include "portaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (512)
#define NUM_SECONDS (10)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG (0)

/* Select sample format. */
#if 1
#    define PA_SAMPLE_TYPE paFloat32
#    define SAMPLE_SIZE (4)
#    define SAMPLE_SILENCE (0.f)
#    define PRINTF_S_FORMAT "%.8f"
#elif 0
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


int main(int argc, char **argv)
{
    rawr_pa_setup();
    rawr_term_setup();

    mn_thread_sleep_ms(50);

    rawr_term_set_input_name(rawr_pa_dev_name(rawr_pa_dev_default_input()));
    rawr_term_set_input_level(0.0);
    rawr_term_set_output_name(rawr_pa_dev_name(rawr_pa_dev_default_output()));
    rawr_term_set_output_level(0.0);

    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream = NULL;
    PaError err;
    const PaDeviceInfo *inputInfo;
    const PaDeviceInfo *outputInfo;
    char *sampleBlock = NULL;
    int i;
    int numBytes;
    int numChannels;
    float max, val;
    double avg;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    inputInfo = Pa_GetDeviceInfo(inputParameters.device);

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputInfo = Pa_GetDeviceInfo(outputParameters.device);

    numChannels = inputInfo->maxInputChannels < outputInfo->maxOutputChannels
                      ? inputInfo->maxInputChannels
                      : outputInfo->maxOutputChannels;

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
        FRAMES_PER_BUFFER,
        paClipOff, /* we won't output out of range samples so don't bother clipping them */
        NULL,      /* no callback, use blocking API */
        NULL);     /* no callback, so no callback userData */
    if (err != paNoError) goto error2;

    numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
    sampleBlock = (char *)malloc(numBytes);
    if (sampleBlock == NULL) {
        printf("Could not allocate record array.\n");
        goto error1;
    }
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);

    int abc = 123;
    err = Pa_StartStream(stream);
    if (err != paNoError) goto error1;

    uint64_t wait_ns = mn_tstamp_convert(200, MN_TSTAMP_MS, MN_TSTAMP_NS);
    uint64_t tstamp_last = mn_tstamp();
    int avg_count = 0;
    double avg_value = 0.0;
    float smax = 0.f, smin = 0.f;
    while (rawr_term_running()) {
        err = Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);
        if (err) goto xrun;

        float *sample = (float *)sampleBlock;
        float sampleWeight = 1.0f / ((float)FRAMES_PER_BUFFER * numChannels);
        avg = 0.0;
        for (int i = 0; i < FRAMES_PER_BUFFER * numChannels; i++) {
            val = fminf(1.f, fmaxf(0.f, *(sample + i)));
            smax = fmaxf(val, smax);
            smin = fminf(val, smin);
            avg += val;
        }
        avg /= FRAMES_PER_BUFFER * numChannels;

        avg_value += avg;
        avg_count++;

        

        uint64_t tstamp = mn_tstamp();
        if ((tstamp - tstamp_last) > wait_ns) {
            tstamp_last = tstamp;
            avg_value /= (double)avg_count;
            avg_count = 0;
            smax = smin = 0.f;

            double decibal = 20 * log10(avg_value);
            double level = -decibal;
            //decibal *= 400.0;
            level = fmax(0.0, level);
            level = fmin(100.0, level);
            level = 1.0 - (level * 0.01);
            rawr_term_set_input_level(level);
        }
        
    }

    rawr_term_cleanup();
    rawr_pa_cleanup();

    return 0;

xrun:
    printf("err = %d\n", err);
    fflush(stdout);
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }
    free(sampleBlock);
    Pa_Terminate();
    if (err & paInputOverflow)
        fprintf(stderr, "Input Overflow.\n");
    if (err & paOutputUnderflow)
        fprintf(stderr, "Output Underflow.\n");
    return -2;
error1:
    free(sampleBlock);
error2:
    if (stream) {
        Pa_AbortStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    return -1;
}
