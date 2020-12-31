#include "re.h"

#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/log.h"
#include "mn/thread.h"
#include "mn/time.h"
#include "playground_srtp.h"

#include "portaudio.h"
#include "srtp.h"

#include <string.h>

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

void rtp_session_run_sip_sendrecv(void *arg)
{
    mn_log_warning("STARTING: %s:%u <-> %u", "0.0.0.0", re_local_port, re_remote_port);
    rtp_session_bothlegs_run(re_remote_ip, re_local_port, re_remote_port);
    mn_log_warning("receiver thread exiting");
}

void rtp_session_send_thread(void *arg)
{
    if (!re_mb) {
        re_mb = mbuf_alloc(300);
        mbuf_init(re_mb);
        mbuf_resize(re_mb, 200);
        mbuf_write_mem(re_mb, re_buf, RTP_HEADER_SIZE * 3);
    }
    //mn_log_warning("STARTING: %s:%u <-> %u", "0.0.0.0", re_local_port, re_remote_port);

    while (1) {
        re_ts += 960;
        mbuf_reset(re_mb);
        mbuf_resize(re_mb, 200);
        mbuf_write_mem(re_mb, re_buf, RTP_HEADER_SIZE * 3);
        rtp_send(re_rtp, sdp_media_raddr(re_sdp_media), 0, 0, 0x66, re_ts, re_mb);
        mn_thread_sleep_ms(20);
    }
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

    re_printf("rtp: recv %zu bytes from %J\n", mbuf_get_left(mb), src);
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
    struct sa laddr;
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

    sa_set_port(&laddr, 0);
    //sa_set_str(&laddr, re_ext_ip, 0);
    /* add supported SIP transports */
    err |= sip_transp_add(re_sip, SIP_TRANSP_UDP, &laddr);
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


    sa_set_str(&laddr, re_ext_ip, re_local_port);
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
    if (1) {
        const char const *invite_uri = "sip:1000@sip.serverlynx.net";
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
