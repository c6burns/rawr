#include "re.h"

#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"
#include "mn/thread.h"
#include "mn/time.h"
#include "playground_srtp.h"

#include "opus.h"
#include "portaudio.h"
#include "srtp.h"

#include <string.h>

#define UDP_OVERHEAD_BYTES 54
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

#define PA_SAMPLE_TYPE paInt16
#define SAMPLE_SIZE (2)
#define SAMPLE_SILENCE (0)
#define PRINTF_S_FORMAT "%d"

static struct sipsess_sock *re_sess_sock; /* SIP session socket */
static struct sdp_session *re_sdp;        /* SDP session        */
static struct sdp_media *re_sdp_media;    /* SDP media          */
static struct sipsess *re_sess;           /* SIP session        */
static struct sipreg *re_reg;             /* SIP registration   */
static struct sip *re_sip;                /* SIP stack          */
static struct rtp_sock *re_rtp;           /* RTP socket         */

const char *re_registrar = "sip:sip.serverlynx.net";
const char *re_uri = "sip:1001@serverlynx.net";
const char *re_name = "Chris Burns";
const char *re_user = "1001";
const char *re_pass = "422423";
const char *re_ext_ip = "136.60.233.221";

mn_thread_t thread_recv;

uint16_t re_local_port = 15420;
uint16_t re_remote_port = 0;
const char *re_remote_ip = NULL;
int markedfirst = 0;
uint32_t re_ts = 0;
struct mbuf *re_mb = NULL;
uint8_t re_buf[300] = {0};

int pa_init = 0;
PaStream *pa_stream_read = NULL;
PaStream *pa_stream_write = NULL;
OpusEncoder *opus_enc;
OpusDecoder *opus_dec;
opus_int16 *opus_inbuf;
unsigned char opus_packet[MAX_PACKET + 257];
opus_int16 *opus_outbuf;
int opus_frame_size;

uint64_t rtp_bytes_sent, rtp_wait_ns, rtp_tstamp_last;

void rtp_session_run_sip_sendrecv(void *arg)
{
    mn_log_warning("STARTING: %s:%u <-> %u", "0.0.0.0", re_local_port, re_remote_port);
    rtp_session_bothlegs_run(re_remote_ip, re_local_port, re_remote_port);
    mn_log_warning("receiver thread exiting");
}

void rtp_session_send_thread(void *arg)
{
    PaStreamParameters inputParameters;
    PaError err;
    const PaDeviceInfo *inputInfo;
    const PaDeviceInfo *outputInfo;
    char *sampleBlock = NULL;
    int i;
    int numBytes;
    int numChannels;

    int j;

    int sampling_rate = 48000;
    int num_channels = 1;
    int application = OPUS_APPLICATION_VOIP;

    int samp_count = 0;

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

    PaDeviceIndex inputDevice = -1, outputDevice = -1;
    int numDevices;
    const PaDeviceInfo *deviceInfo;

    force_channel = 1;

    /* Generate input data */
    opus_inbuf = (opus_int16 *)malloc(sizeof(*opus_inbuf) * SSAMPLES);
    //generate_music(inbuf, SSAMPLES / 2);

    /* Allocate memory for output data */
    opus_int16 *outbuf = (opus_int16 *)malloc(sizeof(*outbuf) * MAX_FRAME_SAMP * 3);

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
    }

    numChannels = 1;
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

    /* -- setup opus -- */

    opus_enc = opus_encoder_create(sampling_rate, num_channels, application, &err);
    if (err != OPUS_OK || opus_enc == NULL) return;

    if (opus_encoder_ctl(opus_enc, OPUS_SET_BITRATE(bitrate)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_FORCE_CHANNELS(force_channel)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_VBR(vbr)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_VBR_CONSTRAINT(vbr_constraint)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_COMPLEXITY(complexity)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_MAX_BANDWIDTH(max_bw)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_SIGNAL(sig)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_INBAND_FEC(inband_fec)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_PACKET_LOSS_PERC(pkt_loss)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_LSB_DEPTH(lsb_depth)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_PREDICTION_DISABLED(pred_disabled)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_DTX(dtx)) != OPUS_OK) goto cleanup;
    if (opus_encoder_ctl(opus_enc, OPUS_SET_EXPERT_FRAME_DURATION(frame_size_enum)) != OPUS_OK) goto cleanup;

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

    err = Pa_OpenStream(
        &pa_stream_read,
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
        mn_log_error("recv: Could not allocate record array.");
        return;
    }
    memset(sampleBlock, SAMPLE_SILENCE, numBytes);

    err = Pa_StartStream(pa_stream_read);
    if (err != paNoError) return;

    // set up buffer for rtmp packets
    re_mb = mbuf_alloc(300);
    mbuf_init(re_mb);

    uint64_t bytes_sent, wait_ns, tstamp, tstamp_last;
    wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);
    bytes_sent = 0;
    tstamp_last = mn_tstamp();
    while (1) {
        int len, ret, out_samples;

        err = Pa_ReadStream(pa_stream_read, sampleBlock, FRAMES_PER_BUFFER);
        if (err) {
            mn_log_error("send: error reading audio stream");
            break;
        }

        /* Encode data, then decode for sanity check */
        samp_count = 0;
        len = opus_encode(opus_enc, (opus_int16 *)sampleBlock, frame_size, opus_packet, MAX_PACKET);
        if (len < 0 || len > MAX_PACKET) {
            mn_log_error("send: opus_encode() returned %d", len);
            ret = -1;
            break;
        }

        mbuf_reset(re_mb);
        mbuf_resize(re_mb, (size_t)len + RTP_HEADER_SIZE);
        mbuf_advance(re_mb, RTP_HEADER_SIZE);
        mbuf_write_mem(re_mb, opus_packet, len);
        mbuf_advance(re_mb, -len);

        bytes_sent += len + UDP_OVERHEAD_BYTES;
        re_ts += 960;
        rtp_send(re_rtp, sdp_media_raddr(re_sdp_media), 0, 0, 0x74, re_ts, re_mb);

        tstamp = mn_tstamp();
        if ((tstamp - tstamp_last) > wait_ns) {
            mn_log_warning("send rate: %.2f KB/s", (double)bytes_sent / 1024.0);
            tstamp_last = tstamp;
            bytes_sent = 0;
        }
    }

    return;

cleanup:

    mn_log_error("error in sending thread");
}

/* terminate */
static void terminate(void)
{
    /* terminate session */
    re_sess = mem_deref(re_sess);

    /* terminate registration */
    re_reg = mem_deref(re_reg);

    /* wait for pending transactions to finish */
    sip_close(re_sip, false);
}

/* called for every received RTP packet */
static void rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
    (void)hdr;
    (void)arg;

    if (!pa_init) {
        pa_init = 1;

        PaStreamParameters outputParameters;
        PaError err;
        const PaDeviceInfo *inputInfo;
        const PaDeviceInfo *outputInfo;
        char *sampleBlock = NULL;
        int i;
        int numBytes;
        int numChannels;

        int j;

        int sampling_rate = 48000;
        int num_channels = 1;
        int application = OPUS_APPLICATION_VOIP;

        int samp_count = 0;

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
        int frame_size_enum;

        PaDeviceIndex inputDevice = -1, outputDevice = -1;
        int numDevices;
        const PaDeviceInfo *deviceInfo;

        opus_frame_size = frame_size_ms_x2 * sampling_rate / 2000;
        frame_size_enum = get_frame_size_enum(opus_frame_size, sampling_rate);
        force_channel = 1;

        /* Generate input data */
        opus_inbuf = (opus_int16 *)malloc(sizeof(*opus_inbuf) * SSAMPLES);
        //generate_music(inbuf, SSAMPLES / 2);

        /* Allocate memory for output data */
        opus_outbuf = (opus_int16 *)malloc(sizeof(*opus_outbuf) * MAX_FRAME_SAMP * 3);

        numDevices = Pa_GetDeviceCount();
        if (numDevices < 0) {
            printf("ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices);
            err = numDevices;
            return;
        }

        for (i = 0; i < numDevices; i++) {
            deviceInfo = Pa_GetDeviceInfo(i);

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

        opus_dec = opus_decoder_create(sampling_rate, num_channels, &err);
        if (err != OPUS_OK || opus_dec == NULL) return;

        err = Pa_OpenStream(
            &pa_stream_write,
            NULL,
            &outputParameters,
            SAMPLE_RATE,
            opus_frame_size,
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

        err = Pa_StartStream(pa_stream_write);
        if (err != paNoError) return;

        err = Pa_WriteStream(pa_stream_write, sampleBlock, FRAMES_PER_BUFFER);
        if (err) return;

        rtp_wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);
        rtp_bytes_sent = 0;
        rtp_tstamp_last = mn_tstamp();
    }

    //re_printf("rtp: recv %zu bytes from %J\n", mbuf_get_left(mb), src);
    rtp_bytes_sent += mbuf_get_left(mb) + UDP_OVERHEAD_BYTES;
    uint64_t tstamp = mn_tstamp();
    if ((tstamp - rtp_tstamp_last) > rtp_wait_ns) {
        mn_log_warning("recv rate: %.2f KB/s", (double)rtp_bytes_sent / 1024.0);
        rtp_tstamp_last = tstamp;
        rtp_bytes_sent = 0;
    }

    {
        int out_samples, ret;

        out_samples = opus_decode(opus_dec, mbuf_buf(mb), mbuf_get_left(mb), opus_outbuf, MAX_FRAME_SAMP, 0);
        if (out_samples != opus_frame_size) {
            mn_log_error("recv: opus_decode() returned %d", out_samples);
            goto cleanup;
        }

        if (Pa_WriteStream(pa_stream_write, opus_outbuf, FRAMES_PER_BUFFER)) {
            goto cleanup;
        }
    }
    return;

cleanup:
    mn_log_error("error during recv");
}

/* called for every received RTCP packet */
static void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
    (void)arg;

    re_printf("rtcp: recv %s from %J\n", rtcp_type_name(msg->hdr.pt), src);
}

/* called when challenged for credentials */
static int auth_handler(char **user, char **pass, const char *realm, void *arg)
{
    int err = 0;
    (void)realm;
    (void)arg;

    err |= str_dup(user, re_user);
    err |= str_dup(pass, re_pass);

    return err;
}

/* print SDP status */
static void update_media(void)
{
    const struct sdp_format *fmt;

    //memcpy(&re_remote_addr, sdp_media_raddr(re_sdp_media), sizeof(re_remote_addr));
    re_remote_port = ntohs(sdp_media_raddr(re_sdp_media)->u.in.sin_port);
    re_remote_ip = inet_ntoa(sdp_media_raddr(re_sdp_media)->u.in.sin_addr);
    re_printf("SDP peer address: %s:%u\n", re_remote_ip, re_remote_port);

    fmt = sdp_media_rformat(re_sdp_media, "opus");
    if (!fmt) {
        re_printf("no common media format found\n");
        return;
    }

    re_printf("SDP media format: %s/%u/%u (payload type: %u)\n", fmt->name, fmt->srate, fmt->ch, fmt->pt);
}

/*
 * called when an SDP offer is received (got offer: true) or
 * when an offer is to be sent (got_offer: false)
 */
static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg, void *arg)
{
    const bool got_offer = mbuf_get_left(msg->mb);
    int err;
    (void)arg;

    if (got_offer) {

        err = sdp_decode(re_sdp, msg->mb, true);
        if (err) {
            re_fprintf(stderr, "unable to decode SDP offer: %s\n", strerror(err));
            return err;
        }

        re_printf("SDP offer received\n");
        update_media();
    } else {
        re_printf("sending SDP offer\n");
    }

    return sdp_encode(mbp, re_sdp, !got_offer);
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
    int err;
    (void)arg;

    re_printf("SDP answer received\n");

    err = sdp_decode(re_sdp, msg->mb, false);
    if (err) {
        re_fprintf(stderr, "unable to decode SDP answer: %s\n", strerror(err));
        return err;
    }

    update_media();

    return 0;
}

/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
    (void)arg;

    re_printf("session progress: %u %r\n", msg->scode, &msg->reason);
}

/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
    (void)msg;
    (void)arg;

    mn_thread_launch(&thread_recv, rtp_session_send_thread, NULL);

    re_printf("session established\n");
}

/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
    (void)arg;

    if (err)
        re_printf("session closed: %s\n", strerror(err));
    else
        re_printf("session closed: %u %r\n", msg->scode, &msg->reason);

    terminate();
}

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
    struct mbuf *mb;
    bool got_offer;
    int err;
    (void)arg;

    if (re_sess) {
        /* Already in a call */
        (void)sip_treply(NULL, re_sip, msg, 486, "Busy Here");
        return;
    }

    got_offer = (mbuf_get_left(msg->mb) > 0);

    /* Decode SDP offer if incoming INVITE contains SDP */
    if (got_offer) {

        err = sdp_decode(re_sdp, msg->mb, true);
        if (err) {
            re_fprintf(stderr, "unable to decode SDP offer: %s\n", strerror(err));
            goto out;
        }

        update_media();
    }

    /* Encode SDP */
    err = sdp_encode(&mb, re_sdp, !got_offer);
    if (err) {
        re_fprintf(stderr, "unable to encode SDP: %s\n", strerror(err));
        goto out;
    }

    /* Answer incoming call */
    err = sipsess_accept(&re_sess, re_sess_sock, msg, 200, "OK", re_name, "application/sdp", mb, auth_handler, NULL, false, offer_handler, answer_handler, establish_handler, NULL, NULL, close_handler, NULL, NULL);
    mem_deref(mb); /* free SDP buffer */
    if (err) {
        re_fprintf(stderr, "session accept error: %s\n", strerror(err));
        goto out;
    }

out:
    if (err) {
        (void)sip_treply(NULL, re_sip, msg, 500, strerror(err));
    } else {
        re_printf("accepting incoming call from <%r>\n", &msg->from.auri);
        mn_thread_launch(&thread_recv, rtp_session_send_thread, NULL);
    }
}

/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
    (void)arg;

    if (err)
        re_printf("register error: %s\n", strerror(err));
    else
        re_printf("register reply: %u %r\n", msg->scode, &msg->reason);
}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    (void)arg;

    /* stop libre main loop */
    re_cancel();
}

/* called upon reception of SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
    re_printf("terminating on signal %d...\n", sig);

    terminate();
}

int main(int argc, char *argv[])
{
    struct sa nsv[16];
    struct dnsc *dnsc = NULL;
    struct sa laddr, ext_addr;
    uint32_t nsc;
    int err; /* errno return values */

    srtp_err_status_t status;
    int ret;

    mn_log_setup();

    err = Pa_Initialize();
    if (err != paNoError) return -1;

#ifdef RTPW_USE_WINSOCK2
    WORD wVersionRequested = MAKEWORD(2, 0);
    WSADATA wsaData;

    ret = WSAStartup(wVersionRequested, &wsaData);
    if (ret != 0) {
        mn_log_error("error: WSAStartup() failed: %d", ret);
        exit(1);
    }
#endif

    mn_log_trace("Using %s [0x%x]", srtp_get_version_string(), srtp_get_version());

    /* initialize srtp library */
    status = srtp_init();
    if (status) {
        mn_log_trace("error: srtp initialization failed with error code %d", status);
        exit(1);
    }

    mn_thread_setup(&thread_recv);

    /* enable coredumps to aid debugging */
    (void)sys_coredump_set(true);

    /* initialize libre state */
    err = libre_init();
    if (err) {
        re_fprintf(stderr, "re init failed: %s\n", strerror(err));
        goto out;
    }

    nsc = ARRAY_SIZE(nsv);

    /* fetch list of DNS server IP addresses */
    err = dns_srv_get(NULL, 0, nsv, &nsc);
    if (err) {
        re_fprintf(stderr, "unable to get dns servers: %s\n", strerror(err));
        goto out;
    }

    /* create DNS client */
    err = dnsc_alloc(&dnsc, NULL, nsv, nsc);
    if (err) {
        re_fprintf(stderr, "unable to create dns client: %s\n", strerror(err));
        goto out;
    }

    /* create SIP stack instance */
    err = sip_alloc(&re_sip, dnsc, 32, 32, 32, "RAWR v0.9.1", exit_handler, NULL);
    if (err) {
        re_fprintf(stderr, "sip error: %s\n", strerror(err));
        goto out;
    }

    /* fetch local IP address */
    err = net_default_source_addr_get(AF_INET, &laddr);
    if (err) {
        re_fprintf(stderr, "local address error: %s\n", strerror(err));
        goto out;
    }

    /* TODO: grab our external IP here using STUN */
    sa_set_str(&ext_addr, re_ext_ip, 0);

    sa_set_port(&laddr, 0);
    //sa_set_str(&laddr, re_ext_ip, 0);
    /* add supported SIP transports */
    err |= sip_transp_add_ext(re_sip, SIP_TRANSP_UDP, &ext_addr, &laddr);
    //err |= sip_transp_add(re_sip, SIP_TRANSP_UDP, &laddr);
    if (err) {
        re_fprintf(stderr, "transport error: %s\n", strerror(err));
        goto out;
    }

    /* create SIP session socket */
    err = sipsess_listen(&re_sess_sock, re_sip, 32, connect_handler, NULL);
    if (err) {
        re_fprintf(stderr, "session listen error: %s\n", strerror(err));
        goto out;
    }

    /* create the RTP/RTCP socket */
    //net_default_source_addr_get(AF_INET, &laddr);
    err = rtp_listen(&re_rtp, IPPROTO_UDP, &laddr, 16384, 32767, true, rtp_handler, rtcp_handler, NULL);
    if (err) {
        re_fprintf(stderr, "rtp listen error: %m\n", err);
        goto out;
    }
    re_local_port = sa_port(rtp_local(re_rtp));
    re_printf("local RTP port is %u\n", sa_port(rtp_local(re_rtp)));

    //sa_set_str(&laddr, re_ext_ip, re_local_port);
    /* create SDP session */
    err = sdp_session_alloc(&re_sdp, &laddr);
    if (err) {
        re_fprintf(stderr, "sdp session error: %s\n", strerror(err));
        goto out;
    }

    /* add audio sdp media, using port from RTP socket */
    err = sdp_media_add(&re_sdp_media, re_sdp, "audio", sa_port(rtp_local(re_rtp)), "RTP/AVP");
    if (err) {
        re_fprintf(stderr, "sdp media error: %s\n", strerror(err));
        goto out;
    }

    /* add opus sdp media format */
    err = sdp_format_add(NULL, re_sdp_media, false, "116", "opus", 48000, 2, NULL, NULL, NULL, false, NULL);
    if (err) {
        re_fprintf(stderr, "sdp format error: %s\n", strerror(err));
        goto out;
    }

    /* invite provided URI */
    if (0) {
        const char const *invite_uri = "sip:3300@sip.serverlynx.net"; // conference
        //const char const *invite_uri = "sip:9195@sip.serverlynx.net"; // 5s delay echo test
        //const char const *invite_uri = "sip:9196@sip.serverlynx.net"; // echo test
        //const char const *invite_uri = "sip:9197@sip.serverlynx.net"; // tone 1
        //const char const *invite_uri = "sip:9198@sip.serverlynx.net"; // tone 2
        struct mbuf *mb;

        /* create SDP offer */
        err = sdp_encode(&mb, re_sdp, true);
        if (err) {
            re_fprintf(stderr, "sdp encode error: %s\n", strerror(err));
            goto out;
        }

        err = sipsess_connect(&re_sess, re_sess_sock, invite_uri, re_name, re_uri, re_name, NULL, 0, "application/sdp", mb, auth_handler, NULL, false, offer_handler, answer_handler, progress_handler, establish_handler, NULL, NULL, close_handler, NULL, NULL);
        mem_deref(mb); /* free SDP buffer */
        if (err) {
            re_fprintf(stderr, "session connect error: %s\n", strerror(err));
            goto out;
        }

        re_printf("inviting <%s>...\n", invite_uri);
    } else {

        err = sipreg_register(&re_reg, re_sip, re_registrar, re_uri, NULL, re_uri, 60, re_name, NULL, 0, 0, auth_handler, NULL, false, register_handler, NULL, NULL, NULL);
        if (err) {
            re_fprintf(stderr, "register error: %s\n", strerror(err));
            goto out;
        }

        re_printf("registering <%s>...\n", re_uri);
    }

    /* main loop */
    err = re_main(signal_handler);

out:
    /* clean up/free all state */
    mem_deref(re_sdp); /* will also free sdp_media */
    mem_deref(re_rtp);
    mem_deref(re_sess_sock);
    mem_deref(re_sip);
    mem_deref(dnsc);

    /* free libre state */
    libre_close();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
