#include "rawr/error.h"

#include "mn/log.h"
#include "mn/thread.h"

#include "re.h"

/* called upon reception of SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
    re_printf("terminating on signal %d...\n", sig);

    terminate();
}

static void udp_recv_handler(const struct sa *src, struct mbuf *mb, void *arg)
{
    struct stun *stun = arg;
    (void)src;

    (void)stun_recv(stun, mb);
}

void stun_handler(struct stun_msg *msg, void *arg)
{
    int abc = 123;
}

void stun_response_handler(int err, uint16_t scode, const char *reason, const struct stun_msg *msg, void *arg)
{
    struct stun_attr *attr;
    (void)reason;

    re_cancel();

    if (err == ECONNABORTED) {
        return;
    }

    attr = stun_msg_attr(msg, STUN_ATTR_XOR_MAPPED_ADDR);
    if (!err && !attr) {
        return;
    }
}

int main(void)
{
    const struct stun_msg *re_stun_msg;
    struct stun_ctrans *re_sct;
    struct stun *re_stun;
    struct udp_sock *re_udp;
    struct sa sa_srv;

    mn_log_setup();

    RAWR_GUARD_CLEANUP(libre_init());

    RAWR_GUARD_CLEANUP(sa_set_str(&sa_srv, "74.125.197.127", 19302));

    RAWR_GUARD_CLEANUP(stun_alloc(&re_stun, NULL, stun_handler, NULL));

    //RAWR_GUARD_CLEANUP(udp_listen(&re_udp, NULL, udp_recv_handler, re_stun));

    RAWR_GUARD_CLEANUP(stun_request(NULL, re_stun, IPPROTO_UDP, NULL, &sa_srv, 0, STUN_METHOD_BINDING, NULL, 0, false, stun_response_handler, NULL, 1, STUN_ATTR_SOFTWARE, "RAWR!"));

    RAWR_GUARD_CLEANUP(re_main(signal_handler));

    libre_close();

    return 0;

cleanup:
    mn_log_error("ERROR");
    libre_close();

    return 1;
}
