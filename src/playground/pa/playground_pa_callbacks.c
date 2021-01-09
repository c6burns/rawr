/** @file paex_record_file.c
	@ingroup examples_src
	@brief Record input into a file, then playback recorded data from file (Windows only at the moment) 
	@author Robert Bielik
*/
/*
 * $Id: paex_record_file.c 1752 2011-09-08 03:21:55Z philburk $
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

#include "portaudio.h"
#include "rawr/ring.h"

#include "mn/allocator.h"
#include "mn/error.h"
#include "mn/log.h"

#include <stdio.h>
#include <stdlib.h>

static ring_buffer_size_t rbs_min(ring_buffer_size_t a, ring_buffer_size_t b)
{
    return (a < b) ? a : b;
}

#define SAMPLE_RATE (48000)
#define FRAMES_PER_BUFFER (960)
#define NUM_SECONDS (10)
#define NUM_CHANNELS (1)
#define NUM_WRITES_PER_BUFFER (4)
#define DITHER_FLAG (0)

#define PA_SAMPLE_TYPE paInt16
typedef short SAMPLE;

typedef struct
{
    SAMPLE *ringBufferDataTo;
    SAMPLE *ringBufferDataFrom;
    rawr_RingBuffer rbToDevice;
    rawr_RingBuffer rbFromDevice;
    FILE *file;
    void *threadHandle;
} paTestData;

SAMPLE silenceBuffer[FRAMES_PER_BUFFER] = {0};

static int audioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    paTestData *data = (paTestData *)userData;
    if (outputBuffer) {
        ring_buffer_size_t elementsToPlay = rawr_RingBuffer_GetReadAvailable(&data->rbToDevice);
        if (elementsToPlay < framesPerBuffer) {
            memset(outputBuffer, 0, framesPerBuffer * sizeof(SAMPLE));
        } else {
            rawr_RingBuffer_Read(&data->rbToDevice, outputBuffer, framesPerBuffer);
        }
    }

    if (inputBuffer) {
        rawr_RingBuffer_Write(&data->rbFromDevice, inputBuffer, framesPerBuffer);
    }

    if (outputBuffer && inputBuffer) memcpy(outputBuffer, inputBuffer, framesPerBuffer * sizeof(SAMPLE));
    return paContinue;
}

static unsigned NextPowerOf2(unsigned val)
{
    val--;
    val = (val >> 1) | val;
    val = (val >> 2) | val;
    val = (val >> 4) | val;
    val = (val >> 8) | val;
    val = (val >> 16) | val;
    return ++val;
}

int main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    PaError err = paNoError;
    paTestData data = {0};
    unsigned numSamples;
    unsigned numBytes;
    int channels = 1;

    SAMPLE tmpSamples[FRAMES_PER_BUFFER] = {0};

    /* ~500 ms of buffer */
    numSamples = NextPowerOf2((unsigned)(SAMPLE_RATE * 0.5));
    numBytes = numSamples * sizeof(SAMPLE);
    data.ringBufferDataTo = (SAMPLE *)MN_MEM_ACQUIRE(numBytes);
    data.ringBufferDataFrom = (SAMPLE *)MN_MEM_ACQUIRE(numBytes);
    if (!data.ringBufferDataTo || !data.ringBufferDataFrom) {
        mn_log_debug("Could not allocate ring buffer data.");
        goto done;
    }

    if (rawr_RingBuffer_Initialize(&data.rbToDevice, sizeof(SAMPLE), numSamples, data.ringBufferDataTo) < 0) {
        mn_log_debug("Failed to initialize ring buffer. Size is not power of 2 ??");
        goto done;
    }

    if (rawr_RingBuffer_Initialize(&data.rbFromDevice, sizeof(SAMPLE), numSamples, data.ringBufferDataFrom) < 0) {
        mn_log_debug("Failed to initialize ring buffer. Size is not power of 2 ??");
        goto done;
    }

    err = Pa_Initialize();
    if (err != paNoError) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        mn_log_error("Error: No default input device.");
        goto done;
    }

    const PaDeviceInfo *info = Pa_GetDeviceInfo(inputParameters.device);
    inputParameters.channelCount = channels;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = info->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        mn_log_error("Error: No default output device.");
        goto done;
    }
    outputParameters.channelCount = channels;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        audioCallback,
        &data);
    if (err != paNoError) goto done;

    if (stream) {
        err = Pa_StartStream(stream);
        if (err != paNoError) goto done;

        mn_log_debug("Waiting for playback to finish.");

        while ((err = Pa_IsStreamActive(stream)) == 1) {
            ring_buffer_size_t elementsReadable = rawr_RingBuffer_GetReadAvailable(&data.rbFromDevice);
            ring_buffer_size_t elementsWriteable = rawr_RingBuffer_GetWriteAvailable(&data.rbToDevice);
            if (elementsReadable < FRAMES_PER_BUFFER || elementsWriteable < FRAMES_PER_BUFFER) {
                mn_thread_sleep(1);
            } else {
                rawr_RingBuffer_Read(&data.rbFromDevice, tmpSamples, FRAMES_PER_BUFFER);
                rawr_RingBuffer_Write(&data.rbToDevice, tmpSamples, FRAMES_PER_BUFFER);
            }
        }
        if (err < 0) goto done;

        err = Pa_CloseStream(stream);
        if (err != paNoError) goto done;

        mn_log_debug("Done.");
    }

done:
    Pa_Terminate();
    MN_MEM_RELEASE(data.ringBufferDataTo);
    MN_MEM_RELEASE(data.ringBufferDataFrom);
    if (err != paNoError) {
        mn_log_error("An error occurred while using the portaudio stream");
        mn_log_error("Error number: %d", err);
        mn_log_error("Error message: %s", Pa_GetErrorText(err));
        err = 1;
    }
    return err;
}
