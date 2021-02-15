// Copyright: TLM Partners Inc. - 2021

#include "PeerManager.h"
#include "SignalChannel.h"
#include "SignalPeer.h"

#include "re.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define SERVER_PEER_IDLE_TIME 10000
#define SERVER_IDLE_TIME 60000
#define MBUF_MAX_SIZE 1024
#define MBUF_MIN_SIZE 2

enum class MessageID : uint8_t {
    None,
    ChannelID,
    ChannelReady,
    PeerCandidates,
};

/* TCP Socket */
static struct tcp_sock *ts;

static void close_handler(int err, void *arg);

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

static const char *stringFromPeerState(PeerState peerState)
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

static void peer_timer_handler(void *arg)
{
    SignalPeerId *peerId = (SignalPeerId *)arg;
    if (!peerId) {
        re_printf("peer_timer_handler: invalid peerId argument");
        return;
    }

    SignalPeer *signalPeer = PeerManager::Peer(*peerId);
    if (!signalPeer) {
        re_printf("peer_timer_handler: no signalPeer for signalId: %zu", peerId);
        return;
    }

    re_printf("TIME %J\n", &signalPeer->reSA);

    close_handler(0, peerId);
}

/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
    re_printf("terminating on signal %d...\n", sig);

    /* stop libre main loop */
    re_cancel();
}

static void establish_handler(void *arg)
{
    SignalPeerId *peerId = (SignalPeerId *)arg;
    if (!peerId) {
        re_printf("peer_timer_handler: invalid peerId argument");
        return;
    }

    SignalPeer *signalPeer = PeerManager::Peer(*peerId);
    if (!signalPeer) {
        re_printf("peer_timer_handler: no signalPeer for signalId: %zu", peerId);
        return;
    }

    tmr_start(&signalPeer->reTimer, SERVER_PEER_IDLE_TIME, peer_timer_handler, &signalPeer->id);

    re_printf("CONN %J\n", &signalPeer->reSA);
}

static void recv_handler(struct mbuf *mb, void *arg)
{
    SignalPeerId *peerId = (SignalPeerId *)arg;
    if (!peerId) {
        re_printf("recv_handler: invalid peerId argument");
        return;
    }

    SignalPeer *signalPeer = PeerManager::Peer(*peerId);
    if (!signalPeer) {
        re_printf("recv_handler: no signalPeer for signalId: %zu", peerId);
        return;
    }

    tmr_start(&signalPeer->reTimer, SERVER_PEER_IDLE_TIME, peer_timer_handler, &signalPeer->id);

    re_printf("RECV %J -- %zu bytes -- %s\n", &signalPeer->reSA, mbuf_get_left(mb), stringFromPeerState(signalPeer->state));

    if (mbuf_get_left(mb) < MBUF_MIN_SIZE) {
        re_printf("RECV %J -- %zu bytes -- Violated Min Message Size\n", &signalPeer->reSA, mbuf_get_left(mb));
        close_handler(0, peerId);
    } else if (mbuf_get_left(mb) >= MBUF_MAX_SIZE) {
        re_printf("RECV %J -- %zu bytes -- Violated Max Message Size\n", &signalPeer->reSA, mbuf_get_left(mb));
        close_handler(0, peerId);
        return;
    } else if (signalPeer->state == PeerState::None) {
        re_printf("RECV %J -- %zu bytes -- Invalid State\n", &signalPeer->reSA, mbuf_get_left(mb));
        close_handler(0, peerId);
        return;
    } else if (signalPeer->state == PeerState::NoChannel) {
        char channelIdBuffer[MBUF_MAX_SIZE] = {0};

        MessageID msgId = (MessageID)mbuf_read_u8(mb);
        if (msgId != MessageID::ChannelID) {
            re_printf("RECV %J -- Invalid MessageID (%d) for Peer state: %s\n", &signalPeer->reSA, msgId, stringFromPeerState(signalPeer->state));
            close_handler(0, peerId);
            return;
        }

        uint8_t msgLen = mbuf_read_u8(mb);
        if (!msgLen) {
            re_printf("RECV %J -- Invalid Message length (%d)\n", &signalPeer->reSA, msgLen);
            close_handler(0, peerId);
            return;
        }

        int err = mbuf_read_str(mb, channelIdBuffer, msgLen);
        if (err) {
            re_printf("RECV %J -- %zu bytes -- Error reading channelId string: %d length\n", &signalPeer->reSA, mbuf_get_left(mb), msgLen);
            close_handler(0, peerId);
        }

        if (!strlen(channelIdBuffer)) {
            re_printf("RECV %J -- Invalid Channel Id\n", &signalPeer->reSA);
            close_handler(0, peerId);
            return;
        }

        re_printf("RECV %J -- Peer Identified Channel: %s\n", &signalPeer->reSA, channelIdBuffer);

        SignalChannel *signalChannel = PeerManager::Channel(channelIdBuffer);
        if (!signalChannel) {
            signalChannel = PeerManager::CreateChannel(channelIdBuffer);
            signalChannel->peers.insert(signalPeer->id);

            signalPeer->state = PeerState::Valid;
            signalPeer->channelId = signalChannel->id;

            tmr_start(&signalPeer->reTimer, SERVER_PEER_IDLE_TIME, peer_timer_handler, &signalPeer->id);

            re_printf("RECV %J -- Channel Created: %s\n", &signalPeer->reSA, channelIdBuffer);
        } else {
            if (signalChannel->peers.size() >= 2) {
                re_printf("RECV %J -- Channel Full: %s\n", &signalPeer->reSA, channelIdBuffer);
                close_handler(0, peerId);
                return;
            } else {
                re_printf("RECV %J -- Channel Ready: %s\n", &signalPeer->reSA, channelIdBuffer);
                signalPeer->channelId = signalChannel->id;
                signalPeer->state = PeerState::Valid;

                signalChannel->state = ChannelState::Exchange;
                signalChannel->peers.insert(signalPeer->id);

                mbuf_rewind(mb);
                mbuf_write_u8(mb, (uint8_t)MessageID::ChannelReady);
                mbuf_write_u8(mb, (uint8_t)0);
                mbuf_set_pos(mb, 0);
                for (auto channelPeerId : signalChannel->peers) {
                    SignalPeer *channelPeer = PeerManager::Peer(channelPeerId);
                    tmr_start(&channelPeer->reTimer, SERVER_PEER_IDLE_TIME, peer_timer_handler, &channelPeer->id);
                    tcp_send(channelPeer->reConn, mb);
                }
            }
        }
    } else if (signalPeer->state == PeerState::Valid) {
        SignalChannel *signalChannel = PeerManager::Channel(signalPeer->channelId);
        if (!signalChannel) {
            re_printf("RECV %J -- Could not find channelId from signalPeer\n", &signalPeer->reSA);
            close_handler(0, peerId);
        }

        MessageID msgId = (MessageID)mbuf_read_u8(mb);
        if (msgId != MessageID::PeerCandidates) {
            re_printf("RECV %J -- Invalid MessageID (%d) for Peer state: %s\n", &signalPeer->reSA, msgId, stringFromPeerState(signalPeer->state));
            close_handler(0, peerId);
            return;
        }

        uint8_t msgLen = mbuf_read_u8(mb);
        if (msgLen % 6) {
            re_printf("RECV %J -- Invalid Message length (%d)\n", &signalPeer->reSA, msgLen);
            close_handler(0, peerId);
            return;
        }

        for (int i = 0; i < msgLen / 6; i++) {
            struct sa peerCandidate;
            sa_init(&peerCandidate, AF_INET);
            peerCandidate.u.in.sin_addr.S_un.S_addr = mbuf_read_u32(mb);
            peerCandidate.u.in.sin_port = mbuf_read_u16(mb);

            re_printf("RECV %J -- received candidate: %J\n", &signalPeer->reSA, &peerCandidate);
        }

        int sentCandidateCount = 0;
        for (auto channelPeerId : signalChannel->peers) {
            if (channelPeerId == *peerId) continue;

            SignalPeer *channelPeer = PeerManager::Peer(channelPeerId);
            if (!channelPeer) {
                re_printf("RECV %J -- could not find channel peer\n", &signalPeer->reSA);
                close_handler(0, peerId);
                return;
            }

            if (channelPeer->state == PeerState::SentCandidates) {
                sentCandidateCount++;
            }

            mbuf_set_pos(mb, 0);
            tcp_send(channelPeer->reConn, mb);
        }

        signalPeer->state = PeerState::SentCandidates;
        sentCandidateCount++;

        if (sentCandidateCount >= 2) {
            re_printf("%s channel peers have now exchanged!\n", signalPeer->channelId.c_str());
            for (auto channelPeerId : signalChannel->peers) {
                SignalPeer *channelPeer = PeerManager::Peer(channelPeerId);
                if (channelPeer) {
                    tmr_start(&channelPeer->reTimer, SERVER_PEER_IDLE_TIME, peer_timer_handler, &channelPeer->id);
                }
            }
        }
    }
}

static void close_handler(int err, void *arg)
{
    SignalPeerId *peerId = (SignalPeerId *)arg;
    if (!peerId) {
        re_printf("close_handler: invalid peerId argument");
        return;
    }

    SignalPeer *signalPeer = PeerManager::Peer(*peerId);
    if (!signalPeer) {
        re_printf("close_handler: no signalPeer for signalId: %zu", peerId);
        return;
    }

    re_printf("DCON %J -- (%s)\n", &signalPeer->reSA, strerror(err));

    PeerManager::DisconnectPeer(*peerId);
    re_printf("%zu connected peers - %zu open channels\n", PeerManager::PeerCount(), PeerManager::ChannelCount());
}

static void connect_handler(const struct sa *peer, void *arg)
{
    struct tcp_conn *tcpConn;
    int err;
    (void)arg;

    SignalPeer *signalPeer = PeerManager::CreatePeer(peer);
    err = tcp_accept(&signalPeer->reConn, ts, establish_handler, recv_handler, close_handler, &signalPeer->id);
    if (err) {
        tcp_reject(ts);
        PeerManager::DisconnectPeer(signalPeer->id);
        return;
    }

    re_printf("ACPT %J\n%zu connected peers - %zu open channels\n", peer, PeerManager::PeerCount(), PeerManager::ChannelCount());
}

struct tmr mainTimer;
void server_timer_handler(void *arg)
{
    (void)arg;
    re_cancel();
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

    //tmr_start(&mainTimer, SERVER_IDLE_TIME, server_timer_handler, NULL);

    /* main loop */
    err = re_main(signal_handler);

out:
    PeerManager::Shutdown();

    /* free TCP socket */
    ts = (struct tcp_sock *)mem_deref(ts);

    /* free library state */
    libre_close();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
