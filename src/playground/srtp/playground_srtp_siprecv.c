#include "rawr/audio.h"
#include "rawr/endpoint.h"
#include "rawr/error.h"
#include "rawr/opus.h"

#include "mn/allocator.h"
#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"
#include "mn/thread.h"
#include "mn/time.h"

#include "playground_srtp.h"

#include "re.h"
#include "opus.h"
#include "portaudio.h"
#include "srtp.h"

#include <stdlib.h>
#include <string.h>

#define UDP_OVERHEAD_BYTES 54
#define MAX_PACKET (1500)

static struct sipsess_sock *re_sess_sock; /* SIP session socket */
static struct sdp_session *re_sdp;        /* SDP session        */
static struct sdp_media *re_sdp_media;    /* SDP media          */
static struct sipsess *re_sess;           /* SIP session        */
static struct sipreg *re_reg;             /* SIP registration   */
static struct sip *re_sip;                /* SIP stack          */
static struct rtp_sock *re_rtp;           /* RTP socket         */

const char *re_registrar = "sip:sip.serverlynx.net";
const char *re_uri = "sip:1001@serverlynx.net";
const char *re_name = "ChrisBurns";
const char *re_user = "1001";
const char *re_pass = "422423";
char *re_ext_ip = NULL;

mn_thread_t thread_recv;

uint16_t re_local_port = 15420;
uint16_t re_remote_port = 0;
const char *re_remote_ip = NULL;
int markedfirst = 0;
uint32_t re_ts = 0;
struct mbuf *re_mb = NULL;
uint8_t re_buf[300] = {0};
int re_receiver = 0;

int pa_init = 0;
PaStream *pa_stream_read = NULL;
PaStream *pa_stream_write = NULL;
OpusEncoder *opus_enc;
OpusDecoder *opus_dec;
opus_int16 *opus_inbuf;
unsigned char opus_packet[MAX_PACKET + 257];
opus_int16 *opus_outbuf;
int opus_frame_size;
rawr_AudioDevice *opus_outDevice;
rawr_Codec *opus_decoder;
rawr_AudioStream *opus_stream = NULL;
mn_atomic_t opus_thread_stopping = MN_ATOMIC_INIT(0);
mn_atomic_t opus_recv_count = MN_ATOMIC_INIT(0);

uint64_t rtp_bytes_sent, rtp_wait_ns, rtp_tstamp_last;

static void terminate(void);

void rtp_session_send_thread(void *arg)
{
    rawr_AudioDevice *inDevice;
    rawr_Codec *encoder;
    rawr_AudioStream *stream = NULL;
    PaError err;
    int len, numBytes, sampleCount;
    char *sampleBlock = NULL;
    int frame_size = rawr_Codec_FrameSize(rawr_CodecRate_48k, rawr_CodecTiming_20ms);
    uint64_t bytes_sent, wait_ns, tstamp, tstamp_last, recv_count, last_recv_count, recv_stasis;
    uint8_t rtp_type = 0x74;
    if (re_receiver) rtp_type = 0x66;

    mn_atomic_store(&opus_thread_stopping, 0);

    numBytes = frame_size * sizeof(rawr_AudioSample);
    sampleBlock = (char *)malloc(numBytes);
    if (sampleBlock == NULL) {
        mn_log_error("recv: Could not allocate record array.");
        return;
    }
    memset(sampleBlock, 0, numBytes);

    // set up buffer for rtmp packets
    re_mb = mbuf_alloc(MAX_PACKET);

    inDevice = rawr_AudioDevice_DefaultInput();

    if (rawr_AudioStream_Setup(&stream, rawr_AudioRate_48000, 1, frame_size)) {
        mn_log_error("rawr_AudioStream_Setup failed");
    }

    if (rawr_AudioStream_AddDevice(stream, inDevice)) {
        mn_log_error("rawr_AudioStream_AddDevice failed on input");
    }

    RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&encoder, rawr_CodecType_Encoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Start(stream));

    recv_stasis = 0;
    last_recv_count = recv_count = mn_atomic_load(&opus_recv_count);

    wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);
    bytes_sent = 0;
    tstamp_last = mn_tstamp();
    while (!mn_atomic_load(&opus_thread_stopping)) {
        sampleCount = 0;
        while ((sampleCount = rawr_AudioStream_Read(stream, sampleBlock)) == 0) {}
        RAWR_GUARD_CLEANUP(sampleCount < 0);

        RAWR_GUARD_CLEANUP((len = rawr_Codec_Encode(encoder, sampleBlock, opus_packet)) < 0);

        mbuf_rewind(re_mb);
        mbuf_fill(re_mb, 0, RTP_HEADER_SIZE);
        mbuf_write_mem(re_mb, opus_packet, len);
        mbuf_advance(re_mb, -len);

        bytes_sent += len + UDP_OVERHEAD_BYTES;
        re_ts += 960;

        rtp_send(re_rtp, sdp_media_raddr(re_sdp_media), 0, 0, rtp_type, re_ts, re_mb);

        tstamp = mn_tstamp();
        if ((tstamp - tstamp_last) > wait_ns) {
            mn_log_trace("send rate: %.2f KB/s", (double)bytes_sent / 1024.0);
            tstamp_last += wait_ns;
            bytes_sent = 0;

            recv_count = mn_atomic_load(&opus_recv_count);
            if (recv_count == last_recv_count) {
                recv_stasis++;
            } else {
                last_recv_count = recv_count;
                recv_stasis = 0;
            }

            if (recv_stasis >= 3) {
                mn_atomic_store(&opus_thread_stopping, 1);
                rawr_AudioStream_Stop(stream);
                rawr_AudioStream_Stop(opus_stream);
                terminate();
                re_cancel();
            }
        }
    }

    mem_deref(re_mb);

    return;

cleanup:

    mem_deref(re_mb);

    mn_log_error("error in sending thread");
}

/* terminate */
static void terminate(void)
{
    mn_log_warning("called");
    /* terminate session */
    re_sess = mem_deref(re_sess);

    /* terminate registration */
    re_reg = mem_deref(re_reg);

    mn_atomic_store(&opus_thread_stopping, 1);

    /* wait for pending transactions to finish */
    sip_close(re_sip, false);
}

static void rawr_rtp_handler(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
    (void)hdr;
    (void)arg;

    if (!pa_init) {
        pa_init = 1;

        uint8_t *silenceSamples = NULL;
        int numBytes;

        int frame_size_enum;
        int application = OPUS_APPLICATION_VOIP;

        opus_frame_size = rawr_Codec_FrameSize(rawr_CodecRate_48k, rawr_CodecTiming_20ms);
        frame_size_enum = rawr_Codec_FrameSizeCode(rawr_CodecRate_48k, opus_frame_size);

        numBytes = sizeof(*opus_inbuf) * opus_frame_size;
        RAWR_GUARD_NULL_CLEANUP(opus_inbuf = MN_MEM_ACQUIRE(numBytes));
        RAWR_GUARD_NULL_CLEANUP(opus_outbuf = MN_MEM_ACQUIRE(numBytes));
        RAWR_GUARD_NULL_CLEANUP(silenceSamples = MN_MEM_ACQUIRE(numBytes));
        memset(silenceSamples, 0, numBytes);

        opus_outDevice = rawr_AudioDevice_DefaultOutput();

        if (rawr_AudioStream_Setup(&opus_stream, rawr_AudioRate_48000, 1, opus_frame_size)) {
            mn_log_error("rawr_AudioStream_Setup failed");
        }

        if (rawr_AudioStream_AddDevice(opus_stream, opus_outDevice)) {
            mn_log_error("rawr_AudioStream_AddDevice failed on input");
        }

        RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&opus_decoder, rawr_CodecType_Decoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));

        RAWR_GUARD_CLEANUP(rawr_AudioStream_Start(opus_stream));

        RAWR_GUARD_CLEANUP(rawr_AudioStream_Write(opus_stream, silenceSamples) < 0);
        RAWR_GUARD_CLEANUP(rawr_AudioStream_Write(opus_stream, silenceSamples) < 0);
        RAWR_GUARD_CLEANUP(rawr_AudioStream_Write(opus_stream, silenceSamples) < 0);
        RAWR_GUARD_CLEANUP(rawr_AudioStream_Write(opus_stream, silenceSamples) < 0);

        rtp_wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);
        rtp_bytes_sent = 0;
        rtp_tstamp_last = mn_tstamp();
    }

    rtp_bytes_sent += mbuf_get_left(mb) + UDP_OVERHEAD_BYTES;
    uint64_t tstamp = mn_tstamp();
    if ((tstamp - rtp_tstamp_last) > rtp_wait_ns) {
        mn_log_trace("recv rate: %.2f KB/s", (double)rtp_bytes_sent / 1024.0);
        rtp_tstamp_last += rtp_wait_ns;
        rtp_bytes_sent = 0;
    }

    RAWR_GUARD_CLEANUP(rawr_Codec_Decode(opus_decoder, mbuf_buf(mb), mbuf_get_left(mb), opus_outbuf) < 0);

    int sampleCount = 0;
    while ((sampleCount = rawr_AudioStream_Write(opus_stream, opus_outbuf)) == 0) {}
    RAWR_GUARD_CLEANUP(sampleCount < 0);

    mn_atomic_store_fetch_add(&opus_recv_count, 1, MN_ATOMIC_ACQ_REL);

    return;

cleanup:
    mn_log_error("error during recv");
}

/* called for every received RTCP packet */
static void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
    (void)src;
    (void)msg;
    (void)arg;
    //mn_log_info("rtcp: recv %s from %J", rtcp_type_name(msg->hdr.pt), src);
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
    mn_log_info("SDP peer address: %s:%u", re_remote_ip, re_remote_port);

    fmt = sdp_media_rformat(re_sdp_media, "opus");
    if (!fmt) {
        mn_log_info("no common media format found");
        return;
    }

    mn_log_info("SDP media format: %s/%u/%u (payload type: %u)", fmt->name, fmt->srate, fmt->ch, fmt->pt);
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

    mn_log_warning("called");

    if (got_offer) {

        err = sdp_decode(re_sdp, msg->mb, true);
        if (err) {
            mn_log_error("unable to decode SDP offer: %s", strerror(err));
            return err;
        }

        mn_log_info("SDP offer received");
        re_receiver = 0;
        update_media();
    } else {
        mn_log_info("sending SDP offer");
    }

    return sdp_encode(mbp, re_sdp, !got_offer);
}

/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
    int err;
    (void)arg;

    mn_log_warning("called: SDP answer received");

    err = sdp_decode(re_sdp, msg->mb, false);
    if (err) {
        mn_log_error("unable to decode SDP answer: %s", strerror(err));
        return err;
    }
    re_receiver = 0;
    update_media();

    return 0;
}

/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
    (void)arg;

    mn_log_info("session progress: %u %r", msg->scode, &msg->reason);
}

/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
    (void)msg;
    (void)arg;

    mn_thread_launch(&thread_recv, rtp_session_send_thread, NULL);

    mn_log_info("session established");
}

/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
    (void)arg;

    if (err)
        mn_log_info("session closed: %s", strerror(err));
    else
        mn_log_info("session closed: %u %r", msg->scode, &msg->reason);

    terminate();
}

/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
    struct mbuf *mb;
    bool got_offer;
    int err;
    (void)arg;

    mn_log_warning("called");

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
            mn_log_error("unable to decode SDP offer: %s", strerror(err));
            goto cleanup;
        }

        re_receiver = 1;
        update_media();
    }

    /* Encode SDP */
    err = sdp_encode(&mb, re_sdp, !got_offer);
    if (err) {
        mn_log_error("unable to encode SDP: %s", strerror(err));
        goto cleanup;
    }

    /* Answer incoming call */
    err = sipsess_accept(&re_sess, re_sess_sock, msg, 200, "OK", re_name, "application/sdp", mb, auth_handler, NULL, false, offer_handler, answer_handler, establish_handler, NULL, NULL, close_handler, NULL, NULL);
    mem_deref(mb); /* free SDP buffer */
    if (err) {
        mn_log_error("session accept error: %s", strerror(err));
        goto cleanup;
    }

cleanup:
    if (err) {
        (void)sip_treply(NULL, re_sip, msg, 500, strerror(err));
    } else {
        mn_log_info("accepting incoming call from <%r>", &msg->from.auri);
        mn_thread_launch(&thread_recv, rtp_session_send_thread, NULL);
    }
}

/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
    (void)arg;

    mn_log_warning("called");

    if (err)
        mn_log_info("register error: %s", strerror(err));
    else
        mn_log_info("register reply: %u %r", msg->scode, &msg->reason);
}

/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
    (void)arg;

    mn_log_warning("called");

    /* stop libre main loop */
    re_cancel();
}

/* called upon reception of SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
    mn_log_info("terminating on signal %d...", sig);

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

    rawr_Endpoint epStunServ, epExternal;

    rawr_Audio_Setup();

    mn_log_setup();

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

    RAWR_GUARD_CLEANUP(libre_init());

    rawr_Endpoint_SetBytes(&epStunServ, 3, 223, 157, 6, 3478);

    rawr_StunClient_BindingRequest(&epStunServ, &epExternal);

    char ipstr[255];
    uint16_t port;
    rawr_Endpoint_String(&epExternal, &port, ipstr, 255);
    mn_log_info("Found external IP via STUN: %s", ipstr);

    nsc = ARRAY_SIZE(nsv);

    /* fetch list of DNS server IP addresses */
    err = dns_srv_get(NULL, 0, nsv, &nsc);
    if (err) {
        mn_log_error("unable to get dns servers: %s", strerror(err));
        goto cleanup;
    }

    /* create DNS client */
    err = dnsc_alloc(&dnsc, NULL, nsv, nsc);
    if (err) {
        mn_log_error("unable to create dns client: %s", strerror(err));
        goto cleanup;
    }

    /* create SIP stack instance */
    err = sip_alloc(&re_sip, dnsc, 32, 32, 32, "RAWR v0.9.1", exit_handler, NULL);
    if (err) {
        mn_log_error("sip error: %s", strerror(err));
        goto cleanup;
    }

    /* fetch local IP address */
    err = net_default_source_addr_get(AF_INET, &laddr);
    if (err) {
        mn_log_error("local address error: %s", strerror(err));
        goto cleanup;
    }

    /* TODO: grab our external IP here using STUN */
    sa_set_sa(&ext_addr, rawr_Endpoint_SockAddr(&epExternal));
    sa_set_port(&laddr, 0);

    /* add supported SIP transports */
    err |= sip_transp_add_ext(re_sip, SIP_TRANSP_UDP, &ext_addr, &laddr);
    //err |= sip_transp_add(re_sip, SIP_TRANSP_UDP, &laddr);
    if (err) {
        mn_log_error("transport error: %s", strerror(err));
        goto cleanup;
    }

    /* create SIP session socket */
    err = sipsess_listen(&re_sess_sock, re_sip, 32, connect_handler, NULL);
    if (err) {
        mn_log_error("session listen error: %s", strerror(err));
        goto cleanup;
    }

    /* create the RTP/RTCP socket */
    net_default_source_addr_get(AF_INET, &laddr);
    err = rtp_listen(&re_rtp, IPPROTO_UDP, &laddr, 16384, 32767, true, rawr_rtp_handler, rtcp_handler, NULL);
    if (err) {
        mn_log_error("rtp listen error: %m", err);
        goto cleanup;
    }
    re_local_port = sa_port(rtp_local(re_rtp));
    mn_log_info("local RTP port is %u", sa_port(rtp_local(re_rtp)));

    sa_set_str(&laddr, re_ext_ip, re_local_port);
    /* create SDP session */
    err = sdp_session_alloc(&re_sdp, &laddr);
    if (err) {
        mn_log_error("sdp session error: %s", strerror(err));
        goto cleanup;
    }

    /* add audio sdp media, using port from RTP socket */
    err = sdp_media_add(&re_sdp_media, re_sdp, "audio", sa_port(rtp_local(re_rtp)), "RTP/AVP");
    if (err) {
        mn_log_error("sdp media error: %s", strerror(err));
        goto cleanup;
    }

    /* add opus sdp media format */
    err = sdp_format_add(NULL, re_sdp_media, false, "116", "opus", 48000, 2, NULL, NULL, NULL, false, NULL);
    if (err) {
        mn_log_error("sdp format error: %s", strerror(err));
        goto cleanup;
    }

    /* invite provided URI */
    if (1) {
        //const char *const invite_uri = "sip:1002@sip.serverlynx.net"; // c6 cell user
        const char *const invite_uri = "sip:3300@sip.serverlynx.net"; // conference
        //const char *const invite_uri = "sip:9195@sip.serverlynx.net"; // 5s delay echo test
        //const char *const invite_uri = "sip:9196@sip.serverlynx.net"; // echo test
        //const char *const invite_uri = "sip:9197@sip.serverlynx.net"; // tone 1
        //const char *const invite_uri = "sip:9198@sip.serverlynx.net"; // tone 2
        struct mbuf *mb;

        /* create SDP offer */
        err = sdp_encode(&mb, re_sdp, true);
        if (err) {
            mn_log_error("sdp encode error: %s", strerror(err));
            goto cleanup;
        }

        err = sipsess_connect(
            &re_sess,
            re_sess_sock,
            invite_uri,
            re_name,
            re_uri,
            re_name,
            NULL,
            0,
            "application/sdp",
            mb,
            auth_handler,
            NULL,
            false,
            offer_handler,
            answer_handler,
            progress_handler,
            establish_handler,
            NULL,
            NULL,
            close_handler,
            NULL,
            NULL
        );
        mem_deref(mb); /* free SDP buffer */
        if (err) {
            mn_log_error("session connect error: %s", strerror(err));
            goto cleanup;
        }

        mn_log_info("inviting <%s>...", invite_uri);
    } else {

        err = sipreg_register(
            &re_reg,
            re_sip,
            re_registrar,
            re_uri,
            NULL,
            re_uri,
            60,
            re_name,
            NULL,
            0,
            0,
            auth_handler,
            NULL,
            false,
            register_handler,
            NULL,
            NULL,
            NULL
        );
        if (err) {
            mn_log_error("register error: %s", strerror(err));
            goto cleanup;
        }

        mn_log_info("registering <%s>...", re_uri);
    }

    /* main loop */
    err = re_main(signal_handler);

cleanup:
    /* clean up/free all state */
    mem_deref(re_sdp); /* will also free sdp_media */
    mem_deref(re_rtp);
    mem_deref(re_sess_sock);
    mem_deref(re_sip);
    mem_deref(dnsc);

    /* free libre state */
    libre_close();

    rawr_Audio_Cleanup();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
