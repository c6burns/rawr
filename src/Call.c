#include "rawr/Call.h"
#include "rawr/Audio.h"
#include "rawr/Endpoint.h"
#include "rawr/Error.h"
#include "rawr/Codec.h"

#include "mn/allocator.h"
#include "mn/atomic.h"
#include "mn/log.h"
#include "mn/thread.h"

#include "re.h"

#define RAWR_CALL_USE_SRTP 1
#define RAWR_CALL_SIPARG_MAX 255
#define RAWR_CALL_UDP_OVERHEAD_BYTES 54

#define RAWR_CALL_SRTP_SUITE SRTP_AES_CM_128_HMAC_SHA1_80

#if RAWR_CALL_USE_SRTP
#   define RAWR_CALL_MEDIA_PROTO "RTP/SAVP"
#   define RAWR_CALL_SRTP_KEY_LEN 16
#   define RAWR_CALL_SRTP_SALT_LEN 14
#   define RAWR_CALL_SRTP_TAG_LEN 10
#else
#   define RAWR_CALL_MEDIA_PROTO "RTP/AVP"
#endif

typedef struct rawr_Call {
    rawr_CallError error;
    rawr_Codec *encoder;
    rawr_Codec *decoder;
    rawr_AudioStream *stream;

    mn_atomic_t state;
    mn_thread_t sipThread;
    mn_atomic_t threadExiting;

    struct sip *reSip;
    struct udp_sock *reRtpSock;
    struct sipreg *reSipReg;
    struct sipsess *reSipSess;
    struct sdp_media *reSdpMedia;
    struct sdp_session *reSdpSess;
    struct sipsess_sock *reSipSessSock;

    char sipInviteURI[RAWR_CALL_SIPARG_MAX];
    char sipURI[RAWR_CALL_SIPARG_MAX];
    char sipName[RAWR_CALL_SIPARG_MAX];
    char sipRegistrar[RAWR_CALL_SIPARG_MAX];
    char sipUsername[RAWR_CALL_SIPARG_MAX];
    char sipPassword[RAWR_CALL_SIPARG_MAX];

    int rtpHandlerIntialized;
    int rtpReceiver;

    mn_thread_t rtpThread;
    size_t rtpBytesSend;
    size_t rtpBytesRecv;
    mn_atomic_t rtpSendTotal;
    mn_atomic_t rtpRecvTotal;
    mn_atomic_t rtpSendRate;
    mn_atomic_t rtpRecvRate;
    uint64_t rtpTime;

    mn_atomic_t rtpRecvCount;
    uint64_t rtpLastRecvTime;

    rawr_AudioSample inputSamples[RAWR_CODEC_OUTPUT_SAMPLES_MAX];
    rawr_AudioSample outputSamples[RAWR_CODEC_INPUT_SAMPLES_MAX];

    enum srtp_suite srtpSuite;
    struct srtp *srtpTransmitContext;
    struct srtp *srtpReceiveContext;
} rawr_Call;

static size_t get_keylen(enum srtp_suite suite)
{
    switch (suite) {
    case SRTP_AES_CM_128_HMAC_SHA1_32: return 16;
    case SRTP_AES_CM_128_HMAC_SHA1_80: return 16;
    case SRTP_AES_256_CM_HMAC_SHA1_32: return 32;
    case SRTP_AES_256_CM_HMAC_SHA1_80: return 32;
    case SRTP_AES_128_GCM:             return 16;
    case SRTP_AES_256_GCM:             return 32;
    default: return 0;
    }
}

static size_t get_saltlen(enum srtp_suite suite)
{
    switch (suite) {
    case SRTP_AES_CM_128_HMAC_SHA1_32: return 14;
    case SRTP_AES_CM_128_HMAC_SHA1_80: return 14;
    case SRTP_AES_256_CM_HMAC_SHA1_32: return 14;
    case SRTP_AES_256_CM_HMAC_SHA1_80: return 14;
    case SRTP_AES_128_GCM:             return 12;
    case SRTP_AES_256_GCM:             return 12;
    default: return 0;
    }
}

static size_t get_taglen(enum srtp_suite suite)
{
    switch (suite) {
    case SRTP_AES_CM_128_HMAC_SHA1_32: return 4;
    case SRTP_AES_CM_128_HMAC_SHA1_80: return 10;
    case SRTP_AES_256_CM_HMAC_SHA1_32: return 4;
    case SRTP_AES_256_CM_HMAC_SHA1_80: return 10;
    case SRTP_AES_128_GCM:             return 16;
    case SRTP_AES_256_GCM:             return 16;
    default: return 0;
    }
}

static void rawr_Call_Terminate(rawr_Call *call);
void rawr_Call_SetState(rawr_Call *call, rawr_CallState callState);
rawr_CallState rawr_Call_State(rawr_Call *call);
void rawr_Call_SetExiting(rawr_Call *call);
int rawr_Call_Exiting(rawr_Call *call);

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_Reset(rawr_Call *call)
{
    RAWR_ASSERT(call);

    mn_atomic_store(&call->threadExiting, 0);

    call->rtpHandlerIntialized = 0;
    call->rtpReceiver = 0;

    call->rtpBytesSend = 0;
    call->rtpBytesRecv = 0;
    call->rtpTime = 0;

    mn_atomic_store(&call->rtpSendTotal, 0);
    mn_atomic_store(&call->rtpRecvTotal, 0);
    mn_atomic_store(&call->rtpSendRate, 0);
    mn_atomic_store(&call->rtpRecvRate, 0);

    mn_atomic_store(&call->rtpRecvCount, 0);
    call->rtpLastRecvTime = 0;

    memset(call->inputSamples, 0, sizeof(call->inputSamples));
    memset(call->outputSamples, 0, sizeof(call->outputSamples));
}


// private thread -----------------------------------------------------------------------------------------------
void rawr_Call_RtpSendThread(void *arg)
{
    int err = 0;
    struct mbuf *re_mb;
    int len, sampleCount;
    char marker;
    rawr_Call *call = (rawr_Call *)arg;
    int frame_size = rawr_Codec_FrameSize(rawr_CodecRate_48k, rawr_CodecTiming_20ms);
    uint64_t rtp_wait_ns, tstamp, tstamp_last, recv_count, last_recv_count, recv_stasis;
    uint32_t rtpSeq, rtpSsrc;
    uint8_t rtp_type = 0x74;
    if (call->rtpReceiver) rtp_type = 0x66;

    // set up buffer for rtmp packets
    re_mb = mbuf_alloc(RAWR_CODEC_OUTPUT_BYTES_MAX);

    recv_stasis = 0;
    last_recv_count = recv_count = mn_atomic_load(&call->rtpRecvCount);

    tstamp_last = mn_tstamp();
    rtp_wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);

    call->rtpBytesSend = 0;
    mn_atomic_store(&call->rtpSendTotal, 0);
    mn_atomic_store(&call->rtpSendRate, 0);

    rtpSeq = 1024;
    rtpSsrc = 0xdead1ee7;

    while (!rawr_Call_Exiting(call)) {
        struct rtp_header hdr;

        RAWR_ASSERT(re_mb->size == RAWR_CODEC_OUTPUT_BYTES_MAX);

        marker = 0;
        call->rtpTime += frame_size;
        if (call->rtpTime == frame_size) marker = 1;

        mbuf_rewind(re_mb);

        hdr.ver = RTP_VERSION;
        hdr.pad = false;
        hdr.ext = 0;
        hdr.cc = 0;
        hdr.m = marker ? 1 : 0;
        hdr.pt = rtp_type;
        hdr.seq = rtpSeq++;
        hdr.ts = call->rtpTime;
        hdr.ssrc = rtpSsrc;

        RAWR_GUARD_CLEANUP(rtp_hdr_encode(re_mb, &hdr));

        sampleCount = 0;
        while ((sampleCount = rawr_AudioStream_Read(call->stream, call->inputSamples)) == 0) {
            mn_thread_sleep_ms(5);
        }
        RAWR_GUARD_CLEANUP(sampleCount < 0);

        RAWR_GUARD_CLEANUP((len = rawr_Codec_Encode(call->encoder, call->inputSamples, mbuf_buf(re_mb))) < 0);

        re_mb->end = re_mb->pos + len;
        re_mb->pos = 0;

        call->rtpBytesSend += len;
        call->rtpBytesSend += RAWR_CALL_UDP_OVERHEAD_BYTES;

#if RAWR_CALL_USE_SRTP
        RAWR_GUARD_CLEANUP(srtp_encrypt(call->srtpTransmitContext, re_mb));
#endif

        RAWR_GUARD_CLEANUP(udp_send(call->reRtpSock, sdp_media_raddr(call->reSdpMedia), re_mb) < 0);

        tstamp = mn_tstamp();
        if ((tstamp - tstamp_last) > rtp_wait_ns) {
            mn_atomic_store_fetch_add(&call->rtpSendTotal, call->rtpBytesSend, MN_ATOMIC_ACQ_REL);
            mn_atomic_store(&call->rtpSendRate, call->rtpBytesSend);

            tstamp_last += rtp_wait_ns;
            call->rtpBytesSend = 0;

            recv_count = mn_atomic_load(&call->rtpRecvCount);
            if (recv_count == last_recv_count) {
                recv_stasis++;
            } else {
                last_recv_count = recv_count;
                recv_stasis = 0;
            }

            if (recv_stasis >= 3) {
                mn_log_warning("RTP recv stasis: %d", recv_stasis);
                //break;
            }
        }
    }

    mem_deref(re_mb);

    return;

cleanup:

    mem_deref(re_mb);

    mn_log_error("error in sending thread");
}

// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnRtp(const struct sa *src, const struct rtp_header *hdr, struct mbuf *mb, void *arg)
{
    (void)hdr;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    uint64_t tstamp;
    uint64_t rtp_wait_ns;

    if (!call->rtpHandlerIntialized) {
        call->rtpHandlerIntialized = 1;
        call->rtpBytesRecv = 0;
        mn_atomic_store(&call->rtpRecvTotal, 0);
        mn_atomic_store(&call->rtpRecvRate, 0);
        call->rtpLastRecvTime = mn_tstamp();
    }

    call->rtpBytesRecv += mbuf_get_left(mb) + RAWR_CALL_UDP_OVERHEAD_BYTES;
    tstamp = mn_tstamp();
    rtp_wait_ns = mn_tstamp_convert(1, MN_TSTAMP_S, MN_TSTAMP_NS);
    if ((tstamp - call->rtpLastRecvTime) > rtp_wait_ns) {
        mn_atomic_store_fetch_add(&call->rtpRecvTotal, call->rtpBytesRecv, MN_ATOMIC_ACQ_REL);
        mn_atomic_store(&call->rtpRecvRate, call->rtpBytesRecv);
        call->rtpLastRecvTime += rtp_wait_ns;
        call->rtpBytesRecv = 0;
    }

#if RAWR_CALL_USE_SRTP
    mb->pos = 0;
    RAWR_GUARD_CLEANUP(srtp_decrypt(call->srtpReceiveContext, mb));
    mb->pos = 12;
#endif
    RAWR_GUARD_CLEANUP(rawr_Codec_Decode(call->decoder, mbuf_buf(mb), mbuf_get_left(mb), &call->outputSamples) < 0);

    int sampleCount = 0;
    while ((sampleCount = rawr_AudioStream_Write(call->stream, &call->outputSamples)) == 0) {}
    RAWR_GUARD_CLEANUP(sampleCount < 0);

    mn_atomic_store_fetch_add(&call->rtpRecvCount, 1, MN_ATOMIC_ACQ_REL);

    return;

cleanup:
    mn_log_error("error during recv");
}

/* called for every received RTCP packet */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnRtcp(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
    (void)src;
    (void)msg;
    (void)arg;
    //mn_log_info("rtcp: recv %s from %J", rtcp_type_name(msg->hdr.pt), src);
}

/* called when challenged for credentials */
// private handler ----------------------------------------------------------------------------------------------
static int rawr_Call_OnAuth(char **user, char **pass, const char *realm, void *arg)
{
    int err = 0;
    (void)realm;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    err |= str_dup(user, call->sipUsername);
    err |= str_dup(pass, call->sipPassword);

    return err;
}

// private handler ----------------------------------------------------------------------------------------------
static bool rawr_Call_CryptoAttribHandler(const char *name, const char *value, void *arg)
{
    rawr_Call *call = (rawr_Call *)arg;

    if (value && !call->srtpReceiveContext) {
        mn_log_debug("SDP crypto: %s", value);

        const char delim[] = " ";
        char *crypto_line = strdup(value);
        char *token = strtok(crypto_line, delim);
        int i = 0;
        const int i_tag = 0, i_suite = 1, i_key = 2;
        while (token != NULL) {
            if (i == i_tag) {
                /* tag */
            } else if (i == i_suite) {
                /* suite */
                const char *suite_name = srtp_suite_name(call->srtpSuite);
                if (strcmp(token, suite_name)) {
                    mn_log_error("Cipher suite mismatch: %s vs %s", token, suite_name);
                    return false;
                }
            } else if (i == i_key) {
                /* key */
                char *inline_key = strchr(token, ':');
                if (inline_key) {
                    inline_key++;
                    mn_log_debug("SDP remote crypto key: %s", inline_key);

                    int err = 0;
                    uint8_t rx_key[RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN];
                    size_t rx_key_len = RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN;
                    err = base64_decode(inline_key, strlen(inline_key), rx_key, &rx_key_len);
                    if (err) {
                        mn_log_error("SDP could not b64 decode crypto key");
                        return false;
                    }

                    err = srtp_alloc(&call->srtpReceiveContext, call->srtpSuite, rx_key, RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN, 0);
                    if (err) {
                        mn_log_error("SDP could not initialize crypto rx context");
                        return false;
                    }

                    return true;
                } else {
                    mn_log_error("SDP could not parse remote key");
                    return false;
                }
            }

            i++;
            token = strtok(NULL, delim);
        }
    }
    return false;
}

/* print SDP status */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_UpdateMedia(void *arg)
{
    const struct sdp_format *fmt;
    char re_remote_ip[32];
    rawr_Call *call = (rawr_Call *)arg;

    const struct sa *raddr = sdp_media_raddr(call->reSdpMedia);
    inet_ntop(sa_af(raddr), &raddr->u.in.sin_addr, re_remote_ip, raddr->len);
    mn_log_debug("SDP peer address: %s:%u", re_remote_ip, sa_port(raddr));

    fmt = sdp_media_rformat(call->reSdpMedia, "opus");
    if (!fmt) {
        mn_log_error("no common media format found");
        return;
    }

    mn_log_debug("SDP media format: %s/%u/%u (payload type: %u)", fmt->name, fmt->srate, fmt->ch, fmt->pt);

#if RAWR_CALL_USE_SRTP
    /* if we have crypto lines in SDP and no receiving context, we need to parse the correct key */
    if (!call->srtpReceiveContext) {
        sdp_media_rattr_apply(call->reSdpMedia, "crypto", rawr_Call_CryptoAttribHandler, call);
    }
#endif
}

/*
 * called when an SDP offer is received (got offer: true) or
 * when an offer is to be sent (got_offer: false)
 */
// private handler ----------------------------------------------------------------------------------------------
static int rawr_Call_OnOffer(struct mbuf **mbp, const struct sip_msg *msg, void *arg)
{
    const bool got_offer = mbuf_get_left(msg->mb);
    int err;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    if (got_offer) {

        err = sdp_decode(call->reSdpSess, msg->mb, true);
        if (err) {
            mn_log_error("unable to decode SDP offer: %s", strerror(err));
            return err;
        }

        mn_log_info("SDP offer received");
        call->rtpReceiver = 0;
        rawr_Call_UpdateMedia(call);
    } else {
        mn_log_info("sending SDP offer");
    }

    return sdp_encode(mbp, call->reSdpSess, !got_offer);
}

/* called when an SDP answer is received */
// private handler ----------------------------------------------------------------------------------------------
static int rawr_Call_OnAnswer(const struct sip_msg *msg, void *arg)
{
    int err;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    mn_log_warning("called: SDP answer received");

    err = sdp_decode(call->reSdpSess, msg->mb, false);
    if (err) {
        mn_log_error("unable to decode SDP answer: %s", strerror(err));
        return err;
    }

    call->rtpReceiver = 0;
    rawr_Call_UpdateMedia(call);

    return 0;
}

/* called when SIP progress (like 180 Ringing) responses are received */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnProgress(const struct sip_msg *msg, void *arg)
{
    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;
    (void)call;

    mn_log_info("session progress: %u", msg->scode);
}

/* called when the session is established */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnEstablish(const struct sip_msg *msg, void *arg)
{
    (void)msg;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    mn_thread_launch(&call->rtpThread, rawr_Call_RtpSendThread, call);

    mn_log_info("session established");
}

/* called when the session fails to connect or is terminated from peer */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnClose(int err, const struct sip_msg *msg, void *arg)
{
    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    if (err)
        mn_log_info("session closed: %s", strerror(err));
    else
        mn_log_info("session closed: %u", msg->scode);

    rawr_Call_Terminate(call);
}

/* called upon incoming calls */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnConnect(const struct sip_msg *msg, void *arg)
{
    struct mbuf *mb;
    bool got_offer;
    int err;

    RAWR_ASSERT(arg);
    rawr_Call *call = (rawr_Call *)arg;

    mn_log_warning("connected");

    if (call->reSipSess) {
        /* Already in a call */
        (void)sip_treply(NULL, call->reSip, msg, 486, "Busy Here");
        return;
    }

    got_offer = (mbuf_get_left(msg->mb) > 0);

    /* Decode SDP offer if incoming INVITE contains SDP */
    if (got_offer) {

        err = sdp_decode(call->reSdpSess, msg->mb, true);
        if (err) {
            mn_log_error("unable to decode SDP offer: %s", strerror(err));
            goto cleanup;
        }

        call->rtpReceiver = 1;
        rawr_Call_UpdateMedia(call);
    }

    /* Encode SDP */
    err = sdp_encode(&mb, call->reSdpSess, !got_offer);
    if (err) {
        mn_log_error("unable to encode SDP: %s", strerror(err));
        goto cleanup;
    }

    /* Answer incoming call */
    err = sipsess_accept(
        &call->reSipSess,
        call->reSipSessSock,
        msg,
        200,
        "OK",
        call->sipName,
        "application/sdp",
        mb,
        rawr_Call_OnAuth,
        call,
        false,
        rawr_Call_OnOffer,
        rawr_Call_OnAnswer,
        rawr_Call_OnEstablish,
        NULL,
        NULL,
        rawr_Call_OnClose,
        call,
        NULL
    );
    mem_deref(mb); /* free SDP buffer */
    if (err) {
        mn_log_error("session accept error: %s", strerror(err));
        goto cleanup;
    }

cleanup:
    if (err) {
        (void)sip_treply(NULL, call->reSip, msg, 500, strerror(err));
    } else {
        mn_log_info("accepting incoming call");
        mn_thread_launch(&call->rtpThread, rawr_Call_RtpSendThread, call);
    }
}

/* called when register responses are received */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnRegister(int err, const struct sip_msg *msg, void *arg)
{
    (void)arg;

    mn_log_warning("called");

    if (err)
        mn_log_info("register error: %s", strerror(err));
    else
        mn_log_info("register reply: %u", msg->scode);
}

/* called when all sip transactions are completed */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnExit(void *arg)
{
    (void)arg;

    mn_log_warning("called");

    /* stop libre main loop */
    re_cancel();
}

/* called upon reception of SIGINT, SIGALRM or SIGTERM */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_OnSignal(int sig)
{
    mn_log_info("terminating on signal %d...", sig);

    re_cancel();
}

/* terminate */
// private ------------------------------------------------------------------------------------------------------
static void rawr_Call_Terminate(rawr_Call *call)
{
    mn_log_warning("terminating");

    /* terminate session */
    call->reSipSess = mem_deref(call->reSipSess);

    /* terminate registration */
    call->reSipReg = mem_deref(call->reSipReg);

    rawr_Call_SetExiting(call);

    /* wait for pending transactions to finish */
    sip_close(call->reSip, false);
}


// private ------------------------------------------------------------------------------------------------------
void rawr_Call_SetState(rawr_Call *call, rawr_CallState callState)
{
    RAWR_ASSERT(call);
    mn_log_debug("%d -> %d", call->state, callState);
    mn_atomic_store(&call->state, callState);
}

// --------------------------------------------------------------------------------------------------------------
rawr_CallState rawr_Call_State(rawr_Call *call)
{
    RAWR_ASSERT(call);
    return (rawr_CallState)mn_atomic_load(&call->state);
}

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_SetExiting(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_atomic_store(&call->threadExiting, 1);
}

// private ------------------------------------------------------------------------------------------------------
int rawr_Call_Exiting(rawr_Call *call)
{
    RAWR_ASSERT(call);
    return (int)mn_atomic_load(&call->threadExiting);
}


void rawr_Call_OnUdpRecv(const struct sa *src, struct mbuf *mb, void *arg)
{
    rawr_Call *call = (rawr_Call *)arg;
    struct rtp_header hdr = {0};
    int err;

    err = rtp_hdr_decode(&hdr, mb);
    if (err) {
        mn_log_error("rtp_hdr_decode err");
    }

    if (RTP_VERSION != hdr.ver) {
        mn_log_error("RTP_VERSION err");
    }

    rawr_Call_OnRtp(src, &hdr, mb, (void *)call);
}

// private thread -----------------------------------------------------------------------------------------------
void rawr_Call_SipThread(void *arg)
{
    RAWR_ASSERT(arg);

    struct sa nsv[16];
    struct dnsc *dnsClient = NULL;
    struct mbuf *mb;
    struct sa localSipSA;
    struct sa localRtpSA;
    uint32_t nameServerCount;
    int err;
    rawr_Call *call = (rawr_Call *)arg;

    rawr_Call_SetState(call, rawr_CallState_Started);

    if (rawr_AudioStream_Setup(&call->stream, rawr_AudioRate_48000, 1, rawr_Codec_FrameSize(rawr_CodecRate_48k, rawr_CodecTiming_20ms))) {
        mn_log_error("rawr_AudioStream_Setup failed");
    }

    if (rawr_AudioStream_AddDevice(call->stream, rawr_AudioDevice_DefaultInput())) {
        mn_log_error("rawr_AudioStream_AddDevice failed on input");
    }

    if (rawr_AudioStream_AddDevice(call->stream, rawr_AudioDevice_DefaultOutput())) {
        mn_log_error("rawr_AudioStream_AddDevice failed on output");
    }

    RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&call->encoder, rawr_CodecType_Encoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));
    RAWR_GUARD_CLEANUP(rawr_Codec_Setup(&call->decoder, rawr_CodecType_Decoder, rawr_CodecRate_48k, rawr_CodecTiming_20ms));

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Start(call->stream));

    nameServerCount = ARRAY_SIZE(nsv);

    /* fetch list of DNS server IP addresses */
    err = dns_srv_get(NULL, 0, nsv, &nameServerCount);
    if (err) {
        mn_log_error("unable to get dns servers: %s", strerror(err));
        goto cleanup;
    }

    /* create DNS client */
    err = dnsc_alloc(&dnsClient, NULL, nsv, nameServerCount);
    if (err) {
        mn_log_error("unable to create dns client: %s", strerror(err));
        goto cleanup;
    }

    /* create SIP stack instance */
    err = sip_alloc(&call->reSip, dnsClient, 32, 32, 32, "RAWR v0.9.1", rawr_Call_OnExit, call);
    if (err) {
        mn_log_error("sip error: %s", strerror(err));
        goto cleanup;
    }

    /* fetch local IP address */
    err = net_default_source_addr_get(AF_INET, &localSipSA);
    if (err) {
        mn_log_error("local address error: %s", strerror(err));
        goto cleanup;
    }

    sa_set_port(&localSipSA, 0);

    /* add supported SIP transports */
    struct tls *sip_tls = NULL;
    err = tls_alloc(&sip_tls, TLS_METHOD_SSLV23, "client.pem", "");
    if (err) {
        re_fprintf(stderr, "tls_alloc error: %s\n", strerror(err));
        goto cleanup;
    }

    err = sip_transp_add(call->reSip, SIP_TRANSP_TLS, &localSipSA, sip_tls);
    //err = sip_transp_add(call->reSip, SIP_TRANSP_UDP, &localSipSA);
    if (err) {
        mn_log_error("transport error: %s", strerror(err));
        goto cleanup;
    }

    /* create SIP session socket */
    err = sipsess_listen(&call->reSipSessSock, call->reSip, 32, rawr_Call_OnConnect, call);
    if (err) {
        mn_log_error("session listen error: %s", strerror(err));
        goto cleanup;
    }

    /* create the RTP/RTCP socket */
    const uint16_t localRtpPort = (rand_u16() % 16383) + 16384;
    net_default_source_addr_get(AF_INET, &localRtpSA);
    sa_set_port(&localRtpSA, localRtpPort);

    re_printf("local SIP address: %J\n", &localSipSA);
    re_printf("local RTP address: %J\n", &localRtpSA);

    err = udp_listen(&call->reRtpSock, &localRtpSA, rawr_Call_OnUdpRecv, (void *)call);
    if (err) {
        mn_log_error("rtp udp_listen error: %m", err);
        goto cleanup;
    }

    /* create SDP session */
    err = sdp_session_alloc(&call->reSdpSess, &localRtpSA);
    if (err) {
        mn_log_error("sdp session error: %s", strerror(err));
        goto cleanup;
    }

    /* add audio sdp media, using port from RTP socket */
    err = sdp_media_add(&call->reSdpMedia, call->reSdpSess, "audio", localRtpPort, RAWR_CALL_MEDIA_PROTO);
    if (err) {
        mn_log_error("sdp media error: %s", strerror(err));
        goto cleanup;
    }

#if RAWR_CALL_USE_SRTP
    /* set up transmit key for SRTP */
    uint8_t tx_key[RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN];
    rand_bytes(tx_key, RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN);
    size_t tx_key_strlen = 255;
    char tx_key_str[256] = "";
    base64_encode(tx_key, RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN, tx_key_str, &tx_key_strlen);

    mn_log_debug("Generated SRTP transmit key: %s", tx_key_str);
    err = srtp_alloc(&call->srtpTransmitContext, call->srtpSuite, tx_key, RAWR_CALL_SRTP_KEY_LEN + RAWR_CALL_SRTP_SALT_LEN, 0);
    if (err) {
        mn_log_error("SDP could not initialize crypto tx context");
    }

    char sdp_crypto_buf[4096];
    snprintf(sdp_crypto_buf, 4095, "1 %s inline:%s", srtp_suite_name(call->srtpSuite), tx_key_str);
    err =  sdp_media_set_lattr(call->reSdpMedia, true, "crypto", sdp_crypto_buf);
    err |= sdp_media_set_lattr(call->reSdpMedia, true, "encryption", "required");
#endif

    /* add opus sdp media format */
    err = sdp_format_add(NULL, call->reSdpMedia, false, "116", "opus", 48000, 2, NULL, NULL, NULL, false, NULL);
    if (err) {
        mn_log_error("sdp format error: %s", strerror(err));
        goto cleanup;
    }

    /* create SDP offer */
    err = sdp_encode(&mb, call->reSdpSess, true);
    if (err) {
        mn_log_error("sdp encode error: %s", strerror(err));
        goto cleanup;
    }

    err = sipsess_connect(
        &call->reSipSess,
        call->reSipSessSock,
        call->sipInviteURI,
        call->sipName,
        call->sipURI,
        call->sipName,
        NULL,
        0,
        "application/sdp",
        mb,
        rawr_Call_OnAuth,
        call,
        false,
        rawr_Call_OnOffer,
        rawr_Call_OnAnswer,
        rawr_Call_OnProgress,
        rawr_Call_OnEstablish,
        NULL,
        NULL,
        rawr_Call_OnClose,
        call,
        NULL
    );

    /*
    err = sipreg_register(
        &call->reSipReg,
        call->reSip,
        call->sipRegistrar,
        call->sipURI,
        call->sipName,
        call->sipURI,
        60,
        call->sipName,
        NULL,
        0,
        0,
        rawr_Call_OnAuth,
        call,
        false,
        rawr_Call_OnRegister,
        call,
        NULL,
        NULL
    );
    */


    mem_deref(mb); /* free SDP buffer */
    if (err) {
        mn_log_error("session connect error: %s", strerror(err));
        goto cleanup;
    }

    mn_log_info("inviting <%s>...", call->sipInviteURI);

    /* execute sip signalling until complete */
    err = re_main(rawr_Call_OnSignal);
    if (err) {
        mn_log_error("re_main exited with error: %s", strerror(err));
        goto cleanup;
    }

    rawr_Call_SetExiting(call);
    rawr_Call_SetState(call, rawr_CallState_Stopping);

    mn_thread_join(&call->rtpThread);

    RAWR_GUARD_CLEANUP(rawr_AudioStream_Stop(call->stream));

    rawr_AudioStream_Cleanup(call->stream);

    rawr_Codec_Cleanup(call->encoder);
    rawr_Codec_Cleanup(call->decoder);

cleanup:

    mem_deref(call->reSdpSess); /* will also free sdp_media */
    mem_deref(call->reRtpSock);
    mem_deref(call->reSipSessSock);
    mem_deref(call->reSip);
    mem_deref(dnsClient);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Setup(rawr_Call **out_call, const char *sipRegistrar, const char *sipURI, const char *sipName, const char *sipUsername, const char *sipPassword)
{
    RAWR_ASSERT(out_call);

    RAWR_GUARD_NULL(*out_call = MN_MEM_ACQUIRE(sizeof(**out_call)));
    memset(*out_call, 0, sizeof(**out_call));

    snprintf((*out_call)->sipURI, RAWR_CALL_SIPARG_MAX, "%s", sipURI);
    snprintf((*out_call)->sipName, RAWR_CALL_SIPARG_MAX, "%s", sipName);
    snprintf((*out_call)->sipRegistrar, RAWR_CALL_SIPARG_MAX, "%s", sipRegistrar);
    snprintf((*out_call)->sipUsername, RAWR_CALL_SIPARG_MAX, "%s", sipUsername);
    snprintf((*out_call)->sipPassword, RAWR_CALL_SIPARG_MAX, "%s", sipPassword);

    (*out_call)->srtpSuite = RAWR_CALL_SRTP_SUITE;

    RAWR_GUARD(mn_thread_setup(&(*out_call)->sipThread));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Call_Cleanup(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_thread_cleanup(&call->sipThread);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Start(rawr_Call *call, const char *sipInviteURI)
{
    RAWR_ASSERT(call);

    rawr_Call_Reset(call);

    snprintf(call->sipInviteURI, RAWR_CALL_SIPARG_MAX, "%s", sipInviteURI);

    rawr_Call_SetState(call, rawr_CallState_Starting);
    RAWR_GUARD(mn_thread_launch(&call->sipThread, rawr_Call_SipThread, (void*)call));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Stop(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_Terminate(call);

    mn_thread_join(&call->sipThread);

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_BlockOnCall(rawr_Call *call)
{
    while (1) {
        int exitLoop = 1;

        rawr_CallState state = rawr_Call_State(call);
        switch (state) {
        case rawr_CallState_Starting:
        case rawr_CallState_Started:
        case rawr_CallState_Connecting:
        case rawr_CallState_Ready:
        case rawr_CallState_Progressing:
        case rawr_CallState_Media:
        case rawr_CallState_Connected:
            exitLoop = 0;
            break;
        case rawr_CallState_None:
        case rawr_CallState_Stopping:
        case rawr_CallState_Stopped:
            break;
        }

        if (exitLoop) break;

        mn_thread_sleep_ms(50);
    }
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
double rawr_Call_InputLevel(rawr_Call *call)
{
    RAWR_ASSERT(call && call->stream);
    return rawr_AudioStream_InputLevel(call->stream);
}

// --------------------------------------------------------------------------------------------------------------
double rawr_Call_OutputLevel(rawr_Call *call)
{
    RAWR_ASSERT(call && call->stream);
    return rawr_AudioStream_OutputLevel(call->stream);
}
