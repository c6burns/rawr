/**
 * @file tc_server.c  TCP Server Demo
 *
 * The demo can for instance be tested with the telnet tool:
 *  $ telnet 127.0.0.1 3456
 *
 * Copyright (C) 2011 Creytiv.com
 */

#include <re.h>

#include <map>
#include <memory>
#include <vector>
#include <string>

#define MBUF_MAX_SIZE 1024

struct SignalPeer;
struct SignalChannel;

static uint64_t hashFromSockAddress(const struct sa *sockAddress);

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

enum class MessageID : uint8_t {
    None,
    ChannelID,
    ChannelReady,
    PeerCandidates,
    Complete,
};

enum class ChannelState : uint8_t {
    None,
    AwaitPeer,
    Exchange,
    Complete,
};

struct SignalChannel {
    std::string id;
    ChannelState state;
    std::map<uint64_t, std::shared_ptr<SignalPeer>> peerMap;

    SignalChannel(std::string newid)
    {
        id = newid;
        state = ChannelState::AwaitPeer;
    }
};

enum class PeerState : uint8_t {
    None,
    NoChannel,
    Valid,
};

struct SignalPeer {
    uint64_t id;
    PeerState state;
    struct conn *reConn;
    std::shared_ptr<SignalChannel> channel;

    SignalPeer(const struct sa *sockAddr, struct conn *newConn)
    {
        id = hashFromSockAddress(sockAddr);
        state = PeerState::NoChannel;
        reConn = newConn;
    }

    ~SignalPeer()
    {
        mem_deref(reConn);
    }
};

static std::map<std::string, std::shared_ptr<SignalChannel>> channelMap;
static std::map<uint64_t, std::shared_ptr<SignalPeer>> peerMap;

static uint64_t hashFromSockAddress(const struct sa *sockAddress)
{
    uint64_t rv = sa_port(sockAddress);
    rv <<= 32;
    rv += sa_in(sockAddress);

    return rv;
}

static const char *stringFromChannelState(ChannelState peerState)
{
    switch (peerState) {
    case ChannelState::None:
        return "ChannelState::None";
    case ChannelState::AwaitPeer:
        return "ChannelState::AwaitPeer";
    case ChannelState::Exchange:
        return "ChannelState::Exchange";
    case ChannelState::Complete:
        return "ChannelState::Complete";
    }
    return "PeerState::Invalid";
}

static const char * stringFromPeerState(PeerState peerState)
{
    switch (peerState) {
    case PeerState::None:
        return "PeerState::None";
    case PeerState::NoChannel:
        return "PeerState::NoChannel";
    case PeerState::Valid:
        return "PeerState::Valid";
    }
    return "PeerState::Invalid";
}

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
    struct conn *conn = (struct conn *)arg;

    /* clean up local TCP connection state */
    mem_deref(conn->tc);
}

static void establish_handler(void *arg)
{
    SignalPeer *signalPeer = (SignalPeer *)arg;
    struct conn *conn = signalPeer->reConn;

    re_printf("CONN %J\n", &conn->peer);
}

static void recv_handler(struct mbuf *mb, void *arg)
{
    SignalPeer *signalPeer = (SignalPeer *)arg;
    struct conn *senderConn = signalPeer->reConn;
    char buffer[1024] = {0};

    re_printf("RECV %J -- %zu bytes -- %s\n", &senderConn->peer, mbuf_get_left(mb), stringFromPeerState(signalPeer->state));

    if (mbuf_get_left(mb) >= MBUF_MAX_SIZE) {
        re_printf("RECV %J -- %zu bytes -- disconnecting for flood", &senderConn->peer, mbuf_get_left(mb));
        peerMap.erase(signalPeer->id);
        return;
    }

    if (signalPeer->state == PeerState::None) {
        peerMap.erase(signalPeer->id);
        return;
    } else if (signalPeer->state == PeerState::NoChannel) {
        for (int i = 0; i < mbuf_get_left(mb); i++) {
            char c = *(mbuf_buf(mb) + i);
            switch (c) {
            case 0:
            case '\r':
            case '\n':
                c = 0;
            }
            buffer[i] = c;

            if (c == 0) break;
        }

        re_printf("RECV %J -- Channel: %s\n", &senderConn->peer, buffer);

        auto it = channelMap.find(buffer);
        if (it == channelMap.end()) {
            signalPeer->state = PeerState::Valid;
            signalPeer->channel = std::shared_ptr<SignalChannel>(new SignalChannel(buffer));
            signalPeer->channel->peerMap.insert(std::make_pair(signalPeer->id, signalPeer));
            channelMap.insert(std::make_pair(buffer, signalPeer->channel));

            re_printf("RECV %J -- Channel state: %s\n", &senderConn->peer, stringFromChannelState(signalPeer->channel->state));
        } else {
            signalPeer->channel = it->second;
            if (signalPeer->channel->peerMap.size() >= 2) {
                re_printf("RECV %J -- Channel full: %s\n", &senderConn->peer, buffer);
                peerMap.erase(signalPeer->id);
                return;
            } else {
                re_printf("RECV %J -- Channel now ready\n", &senderConn->peer);
                signalPeer->state = PeerState::Valid;
                signalPeer->channel->state = ChannelState::Exchange;
                signalPeer->channel->peerMap.insert(std::make_pair(signalPeer->id, signalPeer));

                mbuf_rewind(mb);
                mbuf_write_str(mb, ":D\r\n");
                mbuf_set_pos(mb, 0);
                for (auto it = signalPeer->channel->peerMap.begin(); it != signalPeer->channel->peerMap.end(); ++it) {
                    tcp_send(it->second->reConn->tc, mb);
                }
            }
            
        }
    } else if (signalPeer->state == PeerState::Valid) {
    }
    

    //tcp_send(otherConn->tc, mb);
    //re_printf("SEND %J -- %zu bytes\n", &otherConn->peer, mbuf_get_left(mb));
}

static void close_handler(int err, void *arg)
{
    SignalPeer *signalPeer = (SignalPeer *)arg;
    struct conn *conn = signalPeer->reConn;

    peerMap.erase(signalPeer->id);

    re_printf("DCON %J -- (%s)\n%zu connected peers\n", &conn->peer, strerror(err), peerMap.size());
}

static void connect_handler(const struct sa *peer, void *arg)
{
    struct conn *conn;
    int err;
    (void)arg;

    /* allocate connection state */
    conn = (struct conn *)mem_zalloc(sizeof(*conn), destructor);
    if (!conn) {
        mem_deref(conn);
        tcp_reject(ts);
        return;
    }
    sa_cpy(&conn->peer, peer);

    std::shared_ptr<SignalPeer> signalPeer(new SignalPeer(peer, conn));

    err = tcp_accept(&conn->tc, ts, establish_handler, recv_handler, close_handler, signalPeer.get());
    if (err) {
        tcp_reject(ts);
        return;
    }

    peerMap.insert(std::make_pair(signalPeer->id, signalPeer));

    re_printf("ACPT %J\n%zu connected peers\n", peer, peerMap.size());
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
    ts = (struct tcp_sock *)mem_deref(ts);

    /* free library state */
    libre_close();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
