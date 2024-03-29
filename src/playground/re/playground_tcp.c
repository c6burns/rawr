/**
 * @file tc_server.c  TCP Server Demo
 *
 * The demo can for instance be tested with the telnet tool:
 *  $ telnet 127.0.0.1 3456
 *
 * Copyright (C) 2011 Creytiv.com
 */

#include <re.h>
#include <string.h>

/* Application connection context */
struct conn {
    struct le le;        /* list entry */
    struct sa peer;      /* peer address and port */
    struct tcp_conn *tc; /* TCP connection */
};

/* TCP Socket */
static struct tcp_sock *ts;

/* Linked list of active TCP connections */
static struct list connl;

/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
    re_printf("terminating on signal %d...\n", sig);

    /* stop libre main loop */
    re_cancel();
}

/* destructor called when reference count on conn reach zero */
static void destructor(void *arg)
{
    struct conn *conn = arg;

    /* remove this connection entry from the list */
    list_unlink(&conn->le);

    /* clean up local TCP connection state */
    mem_deref(conn->tc);
}

static void establish_handler(void *arg)
{
    struct conn *conn = arg;

    re_printf("CONN %J\n", &conn->peer);
}

static void recv_handler(struct mbuf *mb, void *arg)
{
    struct conn *otherConn;
    struct conn *senderConn = arg;
    struct le *listEntry;
    int i;

    re_printf("RECV %J -- %zu bytes\n", &senderConn->peer, mbuf_get_left(mb));

    /* received data is echoed back to everyone but the sender */
    for (listEntry = list_head(&connl); listEntry; listEntry = listEntry->next) {
        otherConn = (struct conn *)listEntry->data;

        //if (senderConn == otherConn) continue;

        (void)tcp_send(otherConn->tc, mb);
        re_printf("SEND %J -- %zu bytes\n", &otherConn->peer, mbuf_get_left(mb));
    }
}

static void close_handler(int err, void *arg)
{
    struct conn *conn = arg;

    re_printf("DCON %J -- (%s)\n", &conn->peer, strerror(err));

    /* destroy connection state */
    mem_deref(conn);
}

static void connect_handler(const struct sa *peer, void *arg)
{
    struct conn *conn;
    int err;
    (void)arg;

    /* allocate connection state */
    conn = mem_zalloc(sizeof(*conn), destructor);
    if (!conn) {
        err = ENOMEM;
        goto out;
    }

    /* append connection to connection list */
    list_append(&connl, &conn->le, conn);

    /* save peer address/port */
    conn->peer = *peer;

    /* accept connection */
    err = tcp_accept(&conn->tc, ts, establish_handler, recv_handler, close_handler, conn);
    if (err)
        goto out;

    re_printf("ACPT %J\n", peer);

out:
    if (err) {
        /* destroy state */
        mem_deref(conn);

        /* reject connection */
        tcp_reject(ts);
    }
}

int main(void)
{
    struct sa laddr;
    int err; /* errno return values */

    /* enable coredumps to aid debugging */
    (void)sys_coredump_set(true);

    /* initialize libre state */
    err = libre_init();
    if (err) {
        re_fprintf(stderr, "re init failed: %s\n", strerror(err));
        goto out;
    }

    (void)sa_set_str(&laddr, "0.0.0.0", 3456);

    /* Create listening TCP socket, IP address 0.0.0.0, TCP port 3456 */
    err = tcp_listen(&ts, &laddr, connect_handler, NULL);
    if (err) {
        re_fprintf(stderr, "tcp listen error: %s\n", strerror(err));
        goto out;
    }

    re_printf("LSTN %J\n", &laddr);

    /* main loop */
    err = re_main(signal_handler);

out:
    /* destroy active TCP connections */
    list_flush(&connl);

    /* free TCP socket */
    ts = mem_deref(ts);

    /* free library state */
    libre_close();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
