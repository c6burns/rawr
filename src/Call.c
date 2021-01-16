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

#define RAWR_CALL_SIPARG_MAX 255
#define RAWR_CALL_UDP_OVERHEAD_BYTES 54

typedef struct rawr_Call {
    rawr_CallError error;
    rawr_Codec *encoder;
    rawr_Codec *decoder;
    rawr_AudioStream *stream;

    mn_atomic_t state;
    mn_thread_t sipThread;
    mn_atomic_t threadExiting;

    struct sip *reSip;
    struct rtp_sock *reRtp;
    struct sipreg *reSipReg;
    struct sipsess *reSipSess;
    struct sdp_media *reSdpMedia;
    struct sdp_session *reSdpSess;
    struct sipsess_sock *reSipSessSock;

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
} rawr_Call;


static void rawr_Call_Terminate(rawr_Call *call);
void rawr_Call_SetState(rawr_Call *call, rawr_CallState callState);
rawr_CallState rawr_Call_State(rawr_Call *call);
void rawr_Call_SetExiting(rawr_Call *call);
int rawr_Call_Exiting(rawr_Call *call);


// private thread -----------------------------------------------------------------------------------------------
void rawr_Call_RtpSendThread(void *arg)
{
    struct mbuf *re_mb;
    int len, sampleCount;
    char marker;
    rawr_Call *call = (rawr_Call *)arg;
    int frame_size = rawr_Codec_FrameSize(rawr_CodecRate_48k, rawr_CodecTiming_20ms);
    uint64_t rtp_wait_ns, tstamp, tstamp_last, recv_count, last_recv_count, recv_stasis;
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

    while (!rawr_Call_Exiting(call)) {
        assert(re_mb->size == RAWR_CODEC_OUTPUT_BYTES_MAX);
        mbuf_rewind(re_mb);
        re_mb->pos = RTP_HEADER_SIZE;

        sampleCount = 0;
        while ((sampleCount = rawr_AudioStream_Read(call->stream, call->inputSamples)) == 0) {
            mn_thread_sleep_ms(5);
        }
        RAWR_GUARD_CLEANUP(sampleCount < 0);

        RAWR_GUARD_CLEANUP((len = rawr_Codec_Encode(call->encoder, call->inputSamples, mbuf_buf(re_mb))) < 0);

        re_mb->end = re_mb->pos + len;

        call->rtpBytesSend += len;
        call->rtpBytesSend += RAWR_CALL_UDP_OVERHEAD_BYTES;
        call->rtpTime += frame_size;

        marker = 0;
        if (call->rtpTime == frame_size) marker = 1;

        rtp_send(call->reRtp, sdp_media_raddr(call->reSdpMedia), 0, marker, rtp_type, call->rtpTime, re_mb);

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
                break;
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

/* print SDP status */
// private handler ----------------------------------------------------------------------------------------------
static void rawr_Call_UpdateMedia(void *arg)
{
    const struct sdp_format *fmt;
    char re_remote_ip[16];
    rawr_Call *call = (rawr_Call *)arg;
    const struct sa *sdpSA = sdp_media_raddr(call->reSdpMedia);

    //memcpy(&re_remote_addr, sdp_media_raddr(re_sdp_media), sizeof(re_remote_addr));
    uint16_t re_remote_port = ntohs(sdpSA->u.in.sin_port);
    inet_pton(sa_af(sdpSA), re_remote_ip, (void *)sdpSA);
    mn_log_info("SDP peer address: %s:%u", re_remote_ip, re_remote_port);

    fmt = sdp_media_rformat(call->reSdpMedia, "opus");
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
//static void rawr_Call_OnRegister(int err, const struct sip_msg *msg, void *arg)
//{
//    (void)arg;
//
//    mn_log_warning("called");
//
//    if (err)
//        mn_log_info("register error: %s", strerror(err));
//    else
//        mn_log_info("register reply: %u", msg->scode);
//}

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

// private thread -----------------------------------------------------------------------------------------------
void rawr_Call_SipThread(void *arg)
{
    RAWR_ASSERT(arg);

    struct sa nsv[16];
    struct dnsc *dnsClient = NULL;
    struct mbuf *mb;
    struct sa localAddr;
    uint32_t nameServerCount;
    int err;
    rawr_Call *call = (rawr_Call *)arg;
    const char *sipInviteURI = "sip:3300@sip.serverlynx.net";

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
    err = net_default_source_addr_get(AF_INET, &localAddr);
    if (err) {
        mn_log_error("local address error: %s", strerror(err));
        goto cleanup;
    }

    time_t t;
    srand((unsigned)time(&t));
    sa_set_port(&localAddr, (rand() % 16383) + 16384);

    /* add supported SIP transports */
    err |= sip_transp_add(call->reSip, SIP_TRANSP_UDP, &localAddr);
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
    net_default_source_addr_get(AF_INET, &localAddr);
    err = rtp_listen(&call->reRtp, IPPROTO_UDP, &localAddr, 16384, 32767, true, rawr_Call_OnRtp, rawr_Call_OnRtcp, call);
    if (err) {
        mn_log_error("rtp listen error: %m", err);
        goto cleanup;
    }
    mn_log_info("local RTP port is %u", sa_port(rtp_local(call->reRtp)));

    /* create SDP session */
    err = sdp_session_alloc(&call->reSdpSess, &localAddr);
    if (err) {
        mn_log_error("sdp session error: %s", strerror(err));
        goto cleanup;
    }

    /* add audio sdp media, using port from RTP socket */
    err = sdp_media_add(&call->reSdpMedia, call->reSdpSess, "audio", sa_port(rtp_local(call->reRtp)), "RTP/AVP");
    if (err) {
        mn_log_error("sdp media error: %s", strerror(err));
        goto cleanup;
    }

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
        sipInviteURI,
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
        NULL,
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

    mn_log_info("inviting <%s>...", sipInviteURI);

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
    mem_deref(call->reRtp);
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

    RAWR_GUARD(mn_thread_setup(&(*out_call)->sipThread));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Call_Cleaup(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_thread_cleanup(&call->sipThread);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Start(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_SetState(call, rawr_CallState_Starting);
    RAWR_GUARD(mn_thread_launch(&call->sipThread, rawr_Call_SipThread, (void*)call));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Stop(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_SetExiting(call);

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
