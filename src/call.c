#include "rawr/audio.h"
#include "rawr/call.h"
#include "rawr/endpoint.h"
#include "rawr/error.h"
#include "rawr/opus.h"

#include "mn/atomic.h"
#include "mn/allocator.h"
#include "mn/log.h"
#include "mn/thread.h"

#include "re.h"

#define RAWR_CALL_SIPSTRING_MAX 255

typedef struct rawr_Call {
    mn_atomic_t state;
    rawr_CallError error;
    mn_atomic_t threadExiting;
    mn_thread_t thread;
    rawr_AudioStream *stream;
    rawr_Codec *encoder;
    rawr_Codec *decoder;

    struct sipsess_sock *reSipSessSock;
    struct sdp_session *reSdpSess;
    struct sdp_media *reSdpMedia;
    struct sipsess *reSipSess;
    struct sipreg *reSipReg;
    struct sip *reSip;
    struct rtp_sock *reRtp;

    char sipRegistrar[RAWR_CALL_SIPSTRING_MAX];
    char sipURI[RAWR_CALL_SIPSTRING_MAX];
    char sipName[RAWR_CALL_SIPSTRING_MAX];
    char sipUsername[RAWR_CALL_SIPSTRING_MAX];
    char sipPassword[RAWR_CALL_SIPSTRING_MAX];
} rawr_Call;

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

// private ------------------------------------------------------------------------------------------------------
void rawr_Call_Thread(rawr_Call *call)
{
    RAWR_ASSERT(call);

    struct sa nsv[16];
    struct dnsc *dnsc = NULL;
    struct sa laddr, ext_addr;
    uint32_t nsc;
    int err;

    rawr_Endpoint epStunServ, epExternal;

    rawr_Call_SetState(call, rawr_CallState_Started);

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
    err = sip_alloc(&call->reSip, dnsc, 32, 32, 32, "RAWR v0.9.1", exit_handler, NULL);
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
    //err |= sip_transp_add_ext(re_sip, SIP_TRANSP_UDP, &ext_addr, &laddr);
    err |= sip_transp_add(call->reSip, SIP_TRANSP_UDP, &laddr);
    if (err) {
        mn_log_error("transport error: %s", strerror(err));
        goto cleanup;
    }

    /* create SIP session socket */
    err = sipsess_listen(&call->reSipSessSock, call->reSip, 32, connect_handler, NULL);
    if (err) {
        mn_log_error("session listen error: %s", strerror(err));
        goto cleanup;
    }

    /* create the RTP/RTCP socket */
    net_default_source_addr_get(AF_INET, &laddr);
    err = rtp_listen(&call->reRtp, IPPROTO_UDP, &laddr, 16384, 32767, true, rawr_rtp_handler, rtcp_handler, NULL);
    if (err) {
        mn_log_error("rtp listen error: %m", err);
        goto cleanup;
    }
    mn_log_info("local RTP port is %u", sa_port(rtp_local(call->reRtp)));

    /* create SDP session */
    err = sdp_session_alloc(&call->reSdpSess, &laddr);
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

    while (!rawr_Call_Exiting(call)) {
        int exitLoop = 1;

        rawr_CallState state = rawr_Call_State(call);
        switch (state) {
        case rawr_CallState_Ready:
        case rawr_CallState_Progressing:
        case rawr_CallState_Media:
        case rawr_CallState_Connected:
            exitLoop = 0;
        }

        if (exitLoop) break;

        mn_thread_sleep_ms(50);
    }
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Setup(rawr_Call **out_call, const char *sipRegistrar, const char *sipURI, const char *sipName, const char *sipUsername, const char *sipPassword)
{
    RAWR_ASSERT(out_call);
    RAWR_GUARD_NULL(*out_call = MN_MEM_ACQUIRE(sizeof(**out_call)));
    memset(*out_call, 0, sizeof(**out_call));

    RAWR_GUARD(mn_thread_setup(&(*out_call)->thread));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_Call_Cleaup(rawr_Call *call)
{
    RAWR_ASSERT(call);
    mn_thread_cleanup(&call->thread);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Start(rawr_Call *call)
{
    RAWR_ASSERT(call);

    rawr_Call_SetState(call, rawr_CallState_Starting);
    RAWR_GUARD(mn_thread_launch(&call->thread, rawr_Call_Thread, call));

    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_Stop(rawr_Call *call)
{
    rawr_Call_SetState(call, rawr_CallState_Stopping);
    RAWR_ASSERT(call);
    return rawr_Success;
}

// --------------------------------------------------------------------------------------------------------------
int rawr_Call_BlockOnCall(rawr_Call *call)
{
    while (!rawr_Call_Exiting(call)) {
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
        }

        if (exitLoop) break;

        mn_thread_sleep_ms(50);
    }
    return rawr_Success;
}
