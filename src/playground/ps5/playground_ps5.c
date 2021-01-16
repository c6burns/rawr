#include "rawr/Error.h"

#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

#include <audioin.h>
#include <audioout.h>
#include <kernel.h>
#include <sceerror.h>
#include <user_service.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIO_OUT_GRAIN 960
#define AUDIO_OUT_FREQ 48000
#define N_MULTI_CH 8

#define TRYSEC (10) // 10[sec]
#define TRYCOUNT (SCE_AUDIO_IN_FREQ_48K * TRYSEC / SCE_AUDIO_IN_GRAIN_128)

#define AUDIO_IN_WAV_FORMAT_ID 0x01 // 0x01=16bit int, 0x03=32bit float.

int main(void)
{
    int len;
    int32_t ret = 0;
    int32_t outhandle = 0; // input port handle
    int32_t inhandle = 0;  // output port handle
    int32_t outvol[N_MULTI_CH];
    short *pbuff = NULL;  // pcm main buffer
    short *pptr;          // pointer for pbuff[]
    int32_t inputlen = 0; // 0 to total input samples
    int32_t attainlen;

    ret = sceUserServiceInitialize(NULL);
    if (ret != SCE_OK) {
        goto term;
    }

    /* audio output initialize */
    ret = sceAudioOutInit();
    if ((ret) && (ret != SCE_AUDIO_OUT_ERROR_ALREADY_INIT)) {
        fprintf(stderr, "Failed sceAudioOutInit()\n");
        return 1;
    }	

    mn_log_trace("\n---Start-SoundThread---");

    /* allocate pcm buffer */
    //pbuff = (short *)MN_MEM_ACQUIRE(SCE_AUDIO_IN_GRAIN_128 * sizeof(short) * SCE_AUDIO_IN_CH_STEREO * TRYCOUNT);
    //if (pbuff == NULL) {
    //    mn_log_trace("Error: PCM buffer cannot allocate.");
    //    goto term;
    //}

    SceUserServiceUserId initialUserId;
    ret = sceUserServiceGetInitialUser(&initialUserId);
    if (ret < 0) {
        mn_log_trace("Error: sceUserServiceGetInitialUser() 0x%08x", ret);
        goto term;
    }

    /* Audio Input Test */
    mn_log_trace("Async Audio In Test START");

    /* audio out port open */
    outhandle = sceAudioOutOpen(
        SCE_USER_SERVICE_USER_ID_SYSTEM,
        SCE_AUDIO_OUT_PORT_TYPE_MAIN,
        0,
        AUDIO_OUT_GRAIN,
        AUDIO_OUT_FREQ,
        SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (outhandle < 0) {
        mn_log_error("Error: sceAudioOutOpen() %0x", outhandle);
        goto term;
    }

    /* volume setting */
    outvol[0] = outvol[1] = SCE_AUDIO_VOLUME_0dB;
    sceAudioOutSetVolume(outhandle, (SCE_AUDIO_VOLUME_FLAG_FL_CH | SCE_AUDIO_VOLUME_FLAG_FR_CH), outvol);

    /* Async port open */
    inhandle = sceAudioInAsyncOpen(
        initialUserId,
        SCE_AUDIO_IN_TYPE_GENERAL,
        0,
        AUDIO_OUT_GRAIN,
        SCE_AUDIO_IN_FREQ_48K,
        SCE_AUDIO_IN_PARAM_FORMAT_S16_STEREO);
    if (inhandle < 0) {
        mn_log_error("Error: [0x%08x] sceAudioInAsyncOpen", inhandle);
        goto term;
    }

    /* Audio Input and write to memory (fs=48khz stereo) */
    pptr = pbuff;
    attainlen = 0;
    len = 0;
    for (;;) {
        short tmpbuff[SCE_AUDIO_IN_GRAIN_MAX_ASYNC * SCE_AUDIO_IN_CH_STEREO];
        

        /* printf rec count per sec */
        if (inputlen > attainlen) {
            attainlen += SCE_AUDIO_IN_FREQ_48K;
            mn_log_trace("- REC count %d[sec]", inputlen / SCE_AUDIO_IN_FREQ_48K);
        }

        ret = sceAudioInInput(inhandle, tmpbuff);
        if (ret < 0) {
            mn_log_error("Error: [0x%08x] sceAudioInInput", len);
            break;
        }
        if (ret >= SCE_AUDIO_IN_GRAIN_MAX_ASYNC) {
            mn_log_error("Error: sceAudioInInput() get over length %d", len);
            break;
        }

        len += ret;
        if (len >= AUDIO_OUT_GRAIN) {
            /* audio output */
            ret = sceAudioOutOutput(outhandle, tmpbuff);
            if (ret < 0) {
                mn_log_error("Error: [0x%08x] sceAudioOutput()", ret);
                break;
            }

            len -= ret;
        }
    }

    /* waits for write completion */
    sceAudioInInput(inhandle, NULL);

    /* close port */
    ret = sceAudioInClose(inhandle);
    if (ret < 0) {
        mn_log_error("Error: [0x%08x] sceAudioInClose", ret);
        goto term;
    }

    /* terminate */
    sceAudioOutOutput(outhandle, NULL);

    ret = sceAudioOutClose(outhandle);
    if (ret < 0) {
        mn_log_trace("Error: [0x%08x] sceAudioOutClose()", ret);
        goto term;
    }

term:
    MN_MEM_RELEASE(pbuff);

    mn_log_trace("End-SoundThread");

    ret = sceUserServiceTerminate();
    RAWR_ASSERT(SCE_OK == ret);



    return 0;
}
