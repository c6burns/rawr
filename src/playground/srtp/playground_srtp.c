#include "playground_srtp.h"

#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"
#include "mn/thread.h"
#include "mn/time.h"

#include <errno.h>
#include <signal.h> /* for signal()        */
#include <stdio.h>  /* for printf, fprintf */
#include <stdlib.h> /* for atoi()          */

#include <string.h> /* for strncpy()       */
#include <time.h>   /* for usleep()        */

#ifdef HAVE_UNISTD_H
#    include <unistd.h> /* for close()         */
#elif defined(_MSC_VER)
#    include <io.h> /* for _close()        */
#    define close _close
#endif
#ifdef HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#    include <netinet/in.h>
#elif defined HAVE_WINSOCK2_H
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    define RTPW_USE_WINSOCK2 1
#endif
#ifdef HAVE_ARPA_INET_H
#    include <arpa/inet.h>
#endif

#ifndef _WIN32
#   include <unistd.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#endif

#include "rtp.h"
#include "srtp.h"
#include "util.h"

#include "portaudio.h"

#include "opus.h"

#define DICT_FILE "words.txt"
#define USEC_RATE (5e5)
#define MAX_WORD_LEN 128
#define ADDR_IS_MULTICAST(a) IN_MULTICAST(htonl(a))
#define MAX_KEY_LEN 96

#ifndef HAVE_USLEEP
#    ifdef HAVE_WINDOWS_H
#        define usleep(us) Sleep(((DWORD)us) / 1000)
#    else
#        define usleep(us) sleep((us) / 1000000)
#    endif
#endif

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

mn_atomic_t exiting = MN_ATOMIC_INIT(0);
mn_atomic_t recv_done = MN_ATOMIC_INIT(0);
mn_atomic_t send_done = MN_ATOMIC_INIT(0);

mn_thread_t thread_recv;
mn_thread_t thread_send;



int rtp_session_get_done()
{
    return mn_atomic_load(&recv_done) && mn_atomic_load(&send_done);
}

int rtp_session_get_exiting()
{
    return mn_atomic_load(&exiting);
}

void rtp_session_set_exiting(int val)
{
    mn_atomic_store(&exiting, val);
}


void rtp_session_bothlegs_run(const char *address, uint16_t local_port, uint16_t remote_port)
{
    char *dictfile = DICT_FILE;
    FILE *dict;
    char word[MAX_WORD_LEN];
    int sock, ret = 0;
    struct in_addr rcvr_addr;
    struct sockaddr_in name;
    struct ip_mreq mreq;
#if BEW
    struct sockaddr_in local;
#endif
    srtp_sec_serv_t sec_servs = sec_serv_none;
    unsigned char ttl = 5;
    int c;
    int key_size = 128;
    int tag_size = 8;
    int gcm_on = 0;
    char *input_key = NULL;
    int b64_input = 0;
    char key[MAX_KEY_LEN];
    rtp_sender_t snd;
    rtp_receiver_t rcvr;
    srtp_policy_t policy;
    srtp_err_status_t status;
    int len;
    int expected_len;
    int do_list_mods = 0;
    uint32_t ssrc = 0xdeadbeef; /* ssrc value hardcoded for now */

    memset(&policy, 0x0, sizeof(srtp_policy_t));

    if ((sec_servs && !input_key) || (!sec_servs && input_key)) {
        /*
         * a key must be provided if and only if security services have
         * been requested
         */
        return;
    }

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
    opus_int16 *outbuf;
    int out_samples;

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

    PaDeviceIndex inputDevice = -1, outputDevice = -1;
    int numDevices;
    const PaDeviceInfo *deviceInfo;

    /* Generate input data */
    inbuf = (opus_int16 *)malloc(sizeof(*inbuf) * SSAMPLES);
    //generate_music(inbuf, SSAMPLES / 2);

    /* Allocate memory for output data */
    outbuf = (opus_int16 *)malloc(sizeof(*outbuf) * MAX_FRAME_SAMP * 3);

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        printf("ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices);
        err = numDevices;
        return;
    }

    for (i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);

        /* Mark global and API specific default devices */
        if (i == Pa_GetDefaultInputDevice()) {
            inputDevice = i;
        } else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            if (!inputDevice) inputDevice = i;
        }

        if (i == Pa_GetDefaultOutputDevice()) {
            outputDevice = i;
        } else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultOutputDevice) {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            if (!outputDevice) outputDevice = i;
        }
    }

    mn_log_trace("sizeof(int) = %lu", sizeof(int));
    mn_log_trace("sizeof(long) = %lu", sizeof(long));

    //numChannels = inputInfo->maxInputChannels < outputInfo->maxOutputChannels
    //                  ? inputInfo->maxInputChannels
    //                  : outputInfo->maxOutputChannels;
    numChannels = 1;
    mn_log_trace("Num channels = %d.", numChannels);

    if (inputDevice >= 0) {
        inputInfo = Pa_GetDeviceInfo(inputDevice);
        inputParameters.device = inputDevice;
        inputParameters.channelCount = numChannels;
        inputParameters.sampleFormat = PA_SAMPLE_TYPE;
        inputParameters.suggestedLatency = inputInfo->defaultHighInputLatency;
        inputParameters.hostApiSpecificStreamInfo = NULL;

        mn_log_trace("Input device # %d.", inputParameters.device);
        mn_log_trace("    Name: %s", inputInfo->name);
        mn_log_trace("      LL: %g s", inputInfo->defaultLowInputLatency);
        mn_log_trace("      HL: %g s", inputInfo->defaultHighInputLatency);
    }

    if (outputDevice >= 0) {
        outputInfo = Pa_GetDeviceInfo(outputDevice);
        outputParameters.device = outputDevice;
        outputParameters.channelCount = numChannels;
        outputParameters.sampleFormat = PA_SAMPLE_TYPE;
        outputParameters.suggestedLatency = outputInfo->defaultHighOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = NULL;

        mn_log_trace("Output device # %d.", outputParameters.device);

        mn_log_trace("   Name: %s", outputInfo->name);
        mn_log_trace("     LL: %g s", outputInfo->defaultLowOutputLatency);
        mn_log_trace("     HL: %g s", outputInfo->defaultHighOutputLatency);
    }

    /* -- setup opus -- */

    dec = opus_decoder_create(sampling_rate, num_channels, &err);
    if (err != OPUS_OK || dec == NULL) return;

    enc = opus_encoder_create(sampling_rate, num_channels, application, &err);
    if (err != OPUS_OK || enc == NULL) return;

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

/* set address */
#ifdef HAVE_INET_ATON
    if (0 == inet_aton(address, &rcvr_addr)) {
        mn_log_error("cannot parse IP v4 address %s", address);
        return;
    }
    if (rcvr_addr.s_addr == INADDR_NONE) {
        mn_log_error("address error");
        return;
    }
#else
    rcvr_addr.s_addr = inet_addr(address);
    if (0xffffffff == rcvr_addr.s_addr) {
        mn_log_error("cannot parse IP v4 address %s", address);
        return;
    }
#endif

    /* open socket */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        int err;
#ifdef RTPW_USE_WINSOCK2
        err = WSAGetLastError();

#else
        err = errno;
#endif
        mn_log_error("couldn't open socket: %d", err);
        return;
    }

    memset(&name, 0, sizeof(struct sockaddr_in));
    name.sin_addr = rcvr_addr;
    name.sin_family = PF_INET;
    name.sin_port = htons(remote_port);

    /* report security services selected on the command line */
    mn_log_trace("security services: ");
    if (sec_servs & sec_serv_conf)
        mn_log_trace("confidentiality ");
    if (sec_servs & sec_serv_auth)
        mn_log_trace("message authentication");
    if (sec_servs == sec_serv_none)
        mn_log_trace("none");
    mn_log_trace("");

    /* set up the srtp policy and master key */
    if (sec_servs) {
        /*
         * create policy structure, using the default mechanisms but
         * with only the security services requested on the command line,
         * using the right SSRC value
         */
        switch (sec_servs) {
        case sec_serv_conf_and_auth:
            if (gcm_on) {
#ifdef GCM
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtcp);
                    break;
                }
#else
                mn_log_trace("error: GCM mode only supported when using the OpenSSL "
                             "or NSS crypto engine.");
                return;
#endif
            } else {
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_rtp_default(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                }
            }
            break;
        case sec_serv_conf:
            if (gcm_on) {
                printf(
                    "error: GCM mode must always be used with auth enabled");
                return;
            } else {
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_cm_256_null_auth(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                }
            }
            break;
        case sec_serv_auth:
            if (gcm_on) {
#ifdef GCM
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_gcm_128_8_only_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_128_8_only_auth(
                        &policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_gcm_256_8_only_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_256_8_only_auth(
                        &policy.rtcp);
                    break;
                }
#else
                mn_log_trace("error: GCM mode only supported when using the OpenSSL "
                             "crypto engine.");
                return;
#endif
            } else {
                srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
                srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
            }
            break;
        default:
            mn_log_trace("error: unknown security service requested");
            return;
        }
        policy.ssrc.type = ssrc_specific;
        policy.ssrc.value = ssrc;
        policy.key = (uint8_t *)key;
        policy.ekt = NULL;
        policy.next = NULL;
        policy.window_size = 128;
        policy.allow_repeat_tx = 0;
        policy.rtp.sec_serv = sec_servs;
        policy.rtcp.sec_serv = sec_serv_none; /* we don't do RTCP anyway */

        if (gcm_on && tag_size != 8) {
            policy.rtp.auth_tag_len = tag_size;
        }

        /*
         * read key from hexadecimal or base64 on command line into an octet
         * string
         */
        if (b64_input) {
            int pad;
            expected_len = (policy.rtp.cipher_key_len * 4) / 3;
            len = base64_string_to_octet_string(key, &pad, input_key, expected_len);
            if (pad != 0) {
                mn_log_error("error: padding in base64 unexpected");
                return;
            }
        } else {
            expected_len = policy.rtp.cipher_key_len * 2;
            len = hex_string_to_octet_string(key, input_key, expected_len);
        }
        /* check that hex string is the right length */
        if (len < expected_len) {
            mn_log_error("error: too few digits in key/salt "
                         "(should be %d digits, found %d)",
                         expected_len,
                         len);
            return;
        }
        if ((int)strlen(input_key) > policy.rtp.cipher_key_len * 2) {
            mn_log_error("error: too many digits in key/salt "
                         "(should be %d hexadecimal digits, found %u)",
                         policy.rtp.cipher_key_len * 2,
                         (unsigned)strlen(input_key));
            return;
        }

        mn_log_trace("set master key/salt to %s/", octet_string_hex_string(key, 16));
        mn_log_trace("%s", octet_string_hex_string(key + 16, 14));

    } else {
        /*
         * we're not providing security services, so set the policy to the
         * null policy
         *
         * Note that this policy does not conform to the SRTP
         * specification, since RTCP authentication is required.  However,
         * the effect of this policy is to turn off SRTP, so that this
         * application is now a vanilla-flavored RTP application.
         */
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtp);
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtcp);
        policy.key = (uint8_t *)key;
        policy.ssrc.type = ssrc_specific;
        policy.ssrc.value = ssrc;
        policy.window_size = 0;
        policy.allow_repeat_tx = 0;
        policy.ekt = NULL;
        policy.next = NULL;
    }

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(local_port);
    ret = bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in));
    if (ret < 0) {
        mn_log_error("bind failed");
        perror("");
        return;
    }

    rcvr = rtp_receiver_alloc();
    if (rcvr == NULL) {
        mn_log_error("recv: error: malloc() failed");
        return;
    }
    rtp_receiver_init(rcvr, sock, name, ssrc);
    status = rtp_receiver_init_srtp(rcvr, &policy);
    if (status) {
        mn_log_error("recv: error: srtp_create() failed with code %d", status);
        return;
    }

    err = Pa_OpenStream(
        &stream,
        NULL,
        &outputParameters,
        SAMPLE_RATE,
        frame_size,
        paClipOff, /* we won't output out of range samples so don't bother clipping them */
        NULL,      /* no callback, use blocking API */
        NULL);     /* no callback, so no callback userData */
    if (err != paNoError) return;

    numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
    sampleBlock = (char *)malloc(numBytes);
    if (sampleBlock == NULL) {
        mn_log_error("recv: Could not allocate record array.");
        return;
    }
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);

    err = Pa_StartStream(stream);
    if (err != paNoError) return;

    err = Pa_WriteStream(stream, sampleBlock, FRAMES_PER_BUFFER);
    if (err) return;


    /* initialize sender's rtp and srtp contexts */
    snd = rtp_sender_alloc();
    if (snd == NULL) {
        mn_log_error("send: error: malloc() failed");
        return;
    }
    rtp_sender_init(snd, sock, name, ssrc);
    status = rtp_sender_init_srtp(snd, &policy);
    if (status) {
        mn_log_error("send: error: srtp_create() failed with code %d", status);
        return;
    }

    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        NULL,
        SAMPLE_RATE,
        frame_size,
        paClipOff, /* we won't output out of range samples so don't bother clipping them */
        NULL,      /* no callback, use blocking API */
        NULL);     /* no callback, so no callback userData */
    if (err != paNoError) return;

    numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
    sampleBlock = (char *)malloc(numBytes);
    if (sampleBlock == NULL) {
        mn_log_error("send: Could not allocate record array.");
        return;
    }
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);

    err = Pa_StartStream(stream);
    if (err != paNoError) return;

    uint64_t sleep_ns, tstamp_ns, tstamp_last_ns;
    sleep_ns = mn_tstamp_convert(20, MN_TSTAMP_MS, MN_TSTAMP_NS);
    tstamp_last_ns = 0;
    while (!mn_atomic_load(&exiting)) {
        {
            err = Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);
            if (err) {
                mn_log_error("send: error reading audio stream");
                break;
            }

            /* Encode data, then decode for sanity check */
            samp_count = 0;
            len = opus_encode(enc, (opus_int16 *)sampleBlock, frame_size, packet, MAX_PACKET);
            if (len < 0 || len > MAX_PACKET) {
                mn_log_error("send: opus_encode() returned %d", len);
                ret = -1;
                break;
            }

            out_samples = opus_decode(dec, packet, len, outbuf, MAX_FRAME_SAMP, 0);
            if (out_samples != frame_size) {
                mn_log_error("send: opus_decode() returned %d", out_samples);
                ret = -1;
                break;
            }
            samp_count += frame_size;

            //mn_log_info("send: %d bytes", len);
            rtp_sendto(snd, packet, len);
        }

        {
            len = MAX_PACKET;
            int rbytes = rtp_recvfrom(rcvr, packet, &len);
            if (rbytes < 0) {
                mn_log_error("recv: socket error");
                break;
            } else if (rbytes > 0) {
                mn_log_info("recv: %d bytes", rbytes);
                out_samples = opus_decode(dec, packet, rbytes, outbuf, MAX_FRAME_SAMP, 0);
                if (out_samples != frame_size) {
                    mn_log_error("recv: opus_decode() returned %d", out_samples);
                    ret = -1;
                    break;
                }
                samp_count += frame_size;

                err = Pa_WriteStream(stream, outbuf, FRAMES_PER_BUFFER);
                if (err) break;
            } else {
                err = Pa_WriteStream(stream, sampleBlock, FRAMES_PER_BUFFER);
                if (err) break;
            }
        }
    }

    rtp_receiver_deinit_srtp(rcvr);
    rtp_receiver_dealloc(rcvr);

    rtp_sender_deinit_srtp(snd);
    rtp_sender_dealloc(snd);

    if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
        //leave_group(sock, mreq, argv[0]);
    }

cleanup:

#ifdef RTPW_USE_WINSOCK2
    ret = closesocket(sock);
#else
    ret = close(sock);
#endif
    if (ret < 0) {
        mn_log_error("Failed to close socket");
        perror("");
    }

    return;
}



/*
 * program_type distinguishes the [s]rtp sender and receiver cases
 */


void rtp_session_run(program_type prog_type, const char *address, uint16_t local_port, uint16_t remote_port)
{
    char *dictfile = DICT_FILE;
    FILE *dict;
    char word[MAX_WORD_LEN];
    int sock, ret = 0;
    struct in_addr rcvr_addr;
    struct sockaddr_in name;
    struct ip_mreq mreq;
#if BEW
    struct sockaddr_in local;
#endif
    srtp_sec_serv_t sec_servs = sec_serv_none;
    unsigned char ttl = 5;
    int c;
    int key_size = 128;
    int tag_size = 8;
    int gcm_on = 0;
    char *input_key = NULL;
    int b64_input = 0;
    char key[MAX_KEY_LEN];
    rtp_sender_t snd;
    srtp_policy_t policy;
    srtp_err_status_t status;
    int len;
    int expected_len;
    int do_list_mods = 0;
    uint32_t ssrc = 0xdeadbeef; /* ssrc value hardcoded for now */

    if (prog_type == unknown) {
        mn_log_trace("error: neither sender nor receiver specified");
        return;
    }

    memset(&policy, 0x0, sizeof(srtp_policy_t));

    if ((sec_servs && !input_key) || (!sec_servs && input_key)) {
        /*
         * a key must be provided if and only if security services have
         * been requested
         */
        return;
    }

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
    opus_int16 *outbuf;
    int out_samples;

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

    PaDeviceIndex inputDevice = -1, outputDevice = -1;
    int numDevices;
    const PaDeviceInfo *deviceInfo;

    /* Generate input data */
    inbuf = (opus_int16 *)malloc(sizeof(*inbuf) * SSAMPLES);
    //generate_music(inbuf, SSAMPLES / 2);

    /* Allocate memory for output data */
    outbuf = (opus_int16 *)malloc(sizeof(*outbuf) * MAX_FRAME_SAMP * 3);

    numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        printf("ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices);
        err = numDevices;
        return;
    }

    for (i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);

        /* Mark global and API specific default devices */
        if (i == Pa_GetDefaultInputDevice()) {
            inputDevice = i;
        } else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultInputDevice) {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            if (!inputDevice) inputDevice = i;
        }

        if (i == Pa_GetDefaultOutputDevice()) {
            outputDevice = i;
        } else if (i == Pa_GetHostApiInfo(deviceInfo->hostApi)->defaultOutputDevice) {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            if (!outputDevice) outputDevice = i;
        }
    }

    mn_log_trace("sizeof(int) = %lu", sizeof(int));
    mn_log_trace("sizeof(long) = %lu", sizeof(long));

    //numChannels = inputInfo->maxInputChannels < outputInfo->maxOutputChannels
    //                  ? inputInfo->maxInputChannels
    //                  : outputInfo->maxOutputChannels;
    numChannels = 1;
    mn_log_trace("Num channels = %d.", numChannels);

    if (inputDevice >= 0) {
        inputInfo = Pa_GetDeviceInfo(inputDevice);
        inputParameters.device = inputDevice;
        inputParameters.channelCount = numChannels;
        inputParameters.sampleFormat = PA_SAMPLE_TYPE;
        inputParameters.suggestedLatency = inputInfo->defaultHighInputLatency;
        inputParameters.hostApiSpecificStreamInfo = NULL;

        mn_log_trace("Input device # %d.", inputParameters.device);
        mn_log_trace("    Name: %s", inputInfo->name);
        mn_log_trace("      LL: %g s", inputInfo->defaultLowInputLatency);
        mn_log_trace("      HL: %g s", inputInfo->defaultHighInputLatency);
    }

    if (outputDevice >= 0) {
        outputInfo = Pa_GetDeviceInfo(outputDevice);
        outputParameters.device = outputDevice;
        outputParameters.channelCount = numChannels;
        outputParameters.sampleFormat = PA_SAMPLE_TYPE;
        outputParameters.suggestedLatency = outputInfo->defaultHighOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = NULL;

        mn_log_trace("Output device # %d.", outputParameters.device);
        
        mn_log_trace("   Name: %s", outputInfo->name);
        mn_log_trace("     LL: %g s", outputInfo->defaultLowOutputLatency);
        mn_log_trace("     HL: %g s", outputInfo->defaultHighOutputLatency);
    }

    /* -- setup opus -- */

    dec = opus_decoder_create(sampling_rate, num_channels, &err);
    if (err != OPUS_OK || dec == NULL) return;

    enc = opus_encoder_create(sampling_rate, num_channels, application, &err);
    if (err != OPUS_OK || enc == NULL) return;

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

/* set address */
#ifdef HAVE_INET_ATON
    if (0 == inet_aton(address, &rcvr_addr)) {
        mn_log_error("cannot parse IP v4 address %s", address);
        return;
    }
    if (rcvr_addr.s_addr == INADDR_NONE) {
        mn_log_error("address error");
        return;
    }
#else
    rcvr_addr.s_addr = inet_addr(address);
    if (0xffffffff == rcvr_addr.s_addr) {
        mn_log_error("cannot parse IP v4 address %s", address);
        return;
    }
#endif

    /* open socket */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        int err;
#ifdef RTPW_USE_WINSOCK2
        err = WSAGetLastError();
        
#else
        err = errno;
#endif
        mn_log_error("couldn't open socket: %d", err);
        return;
    }

    memset(&name, 0, sizeof(struct sockaddr_in));
    name.sin_addr = rcvr_addr;
    name.sin_family = PF_INET;
    name.sin_port = htons(remote_port);

    if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
        if (prog_type == sender) {
            ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
            if (ret < 0) {
                mn_log_error("Failed to set TTL for multicast group");
                perror("");
                return;
            }
        }

        mreq.imr_multiaddr.s_addr = rcvr_addr.s_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq));
        if (ret < 0) {
            mn_log_error("Failed to join multicast group");
            perror("");
            return;
        }
    }

    /* report security services selected on the command line */
    mn_log_trace("security services: ");
    if (sec_servs & sec_serv_conf)
        mn_log_trace("confidentiality ");
    if (sec_servs & sec_serv_auth)
        mn_log_trace("message authentication");
    if (sec_servs == sec_serv_none)
        mn_log_trace("none");
    mn_log_trace("");

    /* set up the srtp policy and master key */
    if (sec_servs) {
        /*
         * create policy structure, using the default mechanisms but
         * with only the security services requested on the command line,
         * using the right SSRC value
         */
        switch (sec_servs) {
        case sec_serv_conf_and_auth:
            if (gcm_on) {
#ifdef GCM
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy.rtcp);
                    break;
                }
#else
                mn_log_trace("error: GCM mode only supported when using the OpenSSL "
                             "or NSS crypto engine.");
                return;
#endif
            } else {
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_rtp_default(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                }
            }
            break;
        case sec_serv_conf:
            if (gcm_on) {
                printf(
                    "error: GCM mode must always be used with auth enabled");
                return;
            } else {
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_cm_128_null_auth(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_cm_256_null_auth(&policy.rtp);
                    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
                    break;
                }
            }
            break;
        case sec_serv_auth:
            if (gcm_on) {
#ifdef GCM
                switch (key_size) {
                case 128:
                    srtp_crypto_policy_set_aes_gcm_128_8_only_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_128_8_only_auth(
                        &policy.rtcp);
                    break;
                case 256:
                    srtp_crypto_policy_set_aes_gcm_256_8_only_auth(&policy.rtp);
                    srtp_crypto_policy_set_aes_gcm_256_8_only_auth(
                        &policy.rtcp);
                    break;
                }
#else
                mn_log_trace("error: GCM mode only supported when using the OpenSSL "
                             "crypto engine.");
                return;
#endif
            } else {
                srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
                srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
            }
            break;
        default:
            mn_log_trace("error: unknown security service requested");
            return;
        }
        policy.ssrc.type = ssrc_specific;
        policy.ssrc.value = ssrc;
        policy.key = (uint8_t *)key;
        policy.ekt = NULL;
        policy.next = NULL;
        policy.window_size = 128;
        policy.allow_repeat_tx = 0;
        policy.rtp.sec_serv = sec_servs;
        policy.rtcp.sec_serv = sec_serv_none; /* we don't do RTCP anyway */

        if (gcm_on && tag_size != 8) {
            policy.rtp.auth_tag_len = tag_size;
        }

        /*
         * read key from hexadecimal or base64 on command line into an octet
         * string
         */
        if (b64_input) {
            int pad;
            expected_len = (policy.rtp.cipher_key_len * 4) / 3;
            len = base64_string_to_octet_string(key, &pad, input_key, expected_len);
            if (pad != 0) {
                mn_log_error("error: padding in base64 unexpected");
                return;
            }
        } else {
            expected_len = policy.rtp.cipher_key_len * 2;
            len = hex_string_to_octet_string(key, input_key, expected_len);
        }
        /* check that hex string is the right length */
        if (len < expected_len) {
            mn_log_error("error: too few digits in key/salt "
                         "(should be %d digits, found %d)",
                         expected_len,
                         len);
            return;
        }
        if ((int)strlen(input_key) > policy.rtp.cipher_key_len * 2) {
            mn_log_error("error: too many digits in key/salt "
                         "(should be %d hexadecimal digits, found %u)",
                         policy.rtp.cipher_key_len * 2,
                         (unsigned)strlen(input_key));
            return;
        }

        mn_log_trace("set master key/salt to %s/", octet_string_hex_string(key, 16));
        mn_log_trace("%s", octet_string_hex_string(key + 16, 14));

    } else {
        /*
         * we're not providing security services, so set the policy to the
         * null policy
         *
         * Note that this policy does not conform to the SRTP
         * specification, since RTCP authentication is required.  However,
         * the effect of this policy is to turn off SRTP, so that this
         * application is now a vanilla-flavored RTP application.
         */
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtp);
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtcp);
        policy.key = (uint8_t *)key;
        policy.ssrc.type = ssrc_specific;
        policy.ssrc.value = ssrc;
        policy.window_size = 0;
        policy.allow_repeat_tx = 0;
        policy.ekt = NULL;
        policy.next = NULL;
    }

    if (prog_type == sender) {
        struct sockaddr_in local = {0};
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(local_port);
        ret = bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_in));
        if (ret < 0) {
            mn_log_error("bind failed");
            perror("");
            return;
        }

        /* initialize sender's rtp and srtp contexts */
        snd = rtp_sender_alloc();
        if (snd == NULL) {
            mn_log_error("send: error: malloc() failed");
            return;
        }
        rtp_sender_init(snd, sock, name, ssrc);
        status = rtp_sender_init_srtp(snd, &policy);
        if (status) {
            mn_log_error("send: error: srtp_create() failed with code %d", status);
            return;
        }

        err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,
            SAMPLE_RATE,
            frame_size,
            paClipOff, /* we won't output out of range samples so don't bother clipping them */
            NULL,      /* no callback, use blocking API */
            NULL);     /* no callback, so no callback userData */
        if (err != paNoError) return;

        numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
        sampleBlock = (char *)malloc(numBytes);
        if (sampleBlock == NULL) {
            mn_log_error("send: Could not allocate record array.");
            return;
        }
        memset(sampleBlock, SAMPLE_SILENCE, numBytes);

        err = Pa_StartStream(stream);
        if (err != paNoError) return;

        uint64_t sleep_ns, tstamp_ns, tstamp_last_ns;
        sleep_ns = mn_tstamp_convert(20, MN_TSTAMP_MS, MN_TSTAMP_NS);
        tstamp_last_ns = 0;
        while (!mn_atomic_load(&exiting)) {
            err = Pa_ReadStream(stream, sampleBlock, FRAMES_PER_BUFFER);
            if (err) {
                mn_log_error("send: error reading audio stream");
                break;
            }

            /* Encode data, then decode for sanity check */
            samp_count = 0;
            len = opus_encode(enc, (opus_int16 *)sampleBlock, frame_size, packet, MAX_PACKET);
            if (len < 0 || len > MAX_PACKET) {
                mn_log_error("send: opus_encode() returned %d", len);
                ret = -1;
                break;
            }

            out_samples = opus_decode(dec, packet, len, outbuf, MAX_FRAME_SAMP, 0);
            if (out_samples != frame_size) {
                mn_log_error("send: opus_decode() returned %d", out_samples);
                ret = -1;
                break;
            }
            samp_count += frame_size;

            //mn_log_info("send: %d bytes", len);
            rtp_sendto(snd, packet, len);
        }

        rtp_sender_deinit_srtp(snd);
        rtp_sender_dealloc(snd);
    } else { /* prog_type == receiver */
        rtp_receiver_t rcvr;

        if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
            close(sock);
            mn_log_error("recv: socket bind error");
            perror(NULL);
            if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
                //leave_group(sock, mreq, argv[0]);
            }
            return;
        }

        rcvr = rtp_receiver_alloc();
        if (rcvr == NULL) {
            mn_log_error("recv: error: malloc() failed");
            return;
        }
        rtp_receiver_init(rcvr, sock, name, ssrc);
        status = rtp_receiver_init_srtp(rcvr, &policy);
        if (status) {
            mn_log_error("recv: error: srtp_create() failed with code %d", status);
            return;
        }

        err = Pa_OpenStream(
            &stream,
            NULL,
            &outputParameters,
            SAMPLE_RATE,
            frame_size,
            paClipOff, /* we won't output out of range samples so don't bother clipping them */
            NULL,      /* no callback, use blocking API */
            NULL);     /* no callback, so no callback userData */
        if (err != paNoError) return;

        numBytes = FRAMES_PER_BUFFER * numChannels * SAMPLE_SIZE;
        sampleBlock = (char *)malloc(numBytes);
        if (sampleBlock == NULL) {
            mn_log_error("recv: Could not allocate record array.");
            return;
        }
        memset(sampleBlock, SAMPLE_SILENCE, numBytes);

        err = Pa_StartStream(stream);
        if (err != paNoError) return;

        err = Pa_WriteStream(stream, sampleBlock, FRAMES_PER_BUFFER);
        if (err) return;

        /* get next word and loop */
        while (!mn_atomic_load(&exiting)) {
            len = MAX_PACKET;
            int rbytes = rtp_recvfrom(rcvr, packet, &len);
            if (rbytes < 0) {
                mn_log_error("recv: socket error");
                break;
            } else if (rbytes > 0) {
                mn_log_info("recv: %d bytes", rbytes);
                out_samples = opus_decode(dec, packet, rbytes, outbuf, MAX_FRAME_SAMP, 0);
                if (out_samples != frame_size) {
                    mn_log_error("recv: opus_decode() returned %d", out_samples);
                    ret = -1;
                    break;
                }
                samp_count += frame_size;

                err = Pa_WriteStream(stream, outbuf, FRAMES_PER_BUFFER);
                if (err) break;
            } else {
                err = Pa_WriteStream(stream, sampleBlock, FRAMES_PER_BUFFER);
                if (err) break;
            }
        }

        rtp_receiver_deinit_srtp(rcvr);
        rtp_receiver_dealloc(rcvr);
    }

    if (ADDR_IS_MULTICAST(rcvr_addr.s_addr)) {
        //leave_group(sock, mreq, argv[0]);
    }

cleanup:

#ifdef RTPW_USE_WINSOCK2
    ret = closesocket(sock);
#else
    ret = close(sock);
#endif
    if (ret < 0) {
        mn_log_error("Failed to close socket");
        perror("");
    }

    return;
}

void rtp_session_run_recv(void *arg)
{
    rtp_session_run(receiver, "0.0.0.0", 22452, 22451);
    mn_log_warning("receiver thread exiting");
    mn_atomic_store(&recv_done, 1);
}

void rtp_session_run_send(void *arg)
{
    rtp_session_run(sender, "192.168.21.39", 22451, 22452);
    mn_log_warning("sender thread exiting");
    mn_atomic_store(&send_done, 1);
}

