// Copyright: TLM Partners Inc. - 2021

#include "re.h"

#include <string.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#define CLIENT_TIMER_DELAY 5000
#define CLIENT_LAUNCH_DELAY 10
#define CLIENT_COUNT 100

#define MBUF_MAX_SIZE 1024
#define MBUF_MIN_SIZE 2

enum class MessageID : uint8_t {
    None,
    ChannelID,
    ChannelReady,
    PeerCandidates,
    Complete,
};

enum class ClientState : uint8_t {
    None,
    Connected,
    Exchanging,
    Completed,
    Error,
};

struct SignalClient {
    uint64_t id;
    struct tcp_conn *tcpConn;
    struct tmr reTimer;
    ClientState state;
    bool bCandidatesRcvd;
    bool bCandidatesSent;
    char channelIdString[40];
};

static size_t g_clientsCompleted = 0;

/* TCP Socket */
static struct tcp_sock *ts;

static void close_handler(int err, void *arg);

static const char *stringFromMessageID(MessageID msgID)
{
    switch (msgID) {
    case MessageID::None:
        return "MessageID::None";
    case MessageID::ChannelID:
        return "MessageID::ChannelID";
    case MessageID::ChannelReady:
        return "MessageID::ChannelReady";
    case MessageID::PeerCandidates:
        return "MessageID::PeerCandidates";
    case MessageID::Complete:
        return "MessageID::Complete";
    }
    return "MessageID::Invalid";
}

static const char *stringFromClientState(ClientState state)
{
    switch (state) {
    case ClientState::None:
        return "ClientState::None";
    case ClientState::Connected:
        return "ClientState::Connected";
    case ClientState::Exchanging:
        return "ClientState::Exchanged";
    case ClientState::Completed:
        return "ClientState::Completed";
    case ClientState::Error:
        return "ClientState::Error";
    }
    return "ClientState::Invalid";
}

static void timer_handler(void *arg)
{
    SignalClient *client = (SignalClient *)arg;

    struct mbuf *mb = mbuf_alloc(1024);

    re_printf("%zu - timer_handler - %s\n", client->id, stringFromClientState(client->state));

    if (client->state == ClientState::None) {
        close_handler(0, client);
    } else if (client->state == ClientState::Connected) {
        mbuf_write_u8(mb, (uint8_t)MessageID::ChannelID);
        mbuf_write_u8(mb, (uint8_t)strlen(client->channelIdString));
        mbuf_write_str(mb, client->channelIdString);
        mbuf_set_pos(mb, 0);
        tcp_send(client->tcpConn, mb);
    } else if (client->state == ClientState::Exchanging) {
        struct sa peerCandidate;
        if (!client->bCandidatesSent && client->id == 0) {
            mbuf_write_u8(mb, (uint8_t)MessageID::PeerCandidates);
            mbuf_write_u8(mb, (uint8_t)12);

            sa_set_str(&peerCandidate, "111.111.111.111", 1123);
            mbuf_write_u32(mb, peerCandidate.u.in.sin_addr.s_addr);
            mbuf_write_u16(mb, peerCandidate.u.in.sin_port);

            sa_set_str(&peerCandidate, "112.112.112.112", 2234);
            mbuf_write_u32(mb, peerCandidate.u.in.sin_addr.s_addr);
            mbuf_write_u16(mb, peerCandidate.u.in.sin_port);

            mbuf_set_pos(mb, 0);
            int err = tcp_send(client->tcpConn, mb);
            if (err) {
                re_printf("tcp_send error\n");
                close_handler(0, client);
            }
        } else if (!client->bCandidatesSent && client->id == 1) {
            mbuf_write_u8(mb, (uint8_t)MessageID::PeerCandidates);
            mbuf_write_u8(mb, (uint8_t)12);

            sa_set_str(&peerCandidate, "113.113.113.113", 3345);
            mbuf_write_u32(mb, peerCandidate.u.in.sin_addr.s_addr);
            mbuf_write_u16(mb, peerCandidate.u.in.sin_port);

            sa_set_str(&peerCandidate, "114.114.114.114", 4456);
            mbuf_write_u32(mb, peerCandidate.u.in.sin_addr.s_addr);
            mbuf_write_u16(mb, peerCandidate.u.in.sin_port);

            mbuf_set_pos(mb, 0);
            int err = tcp_send(client->tcpConn, mb);
            if (err) {
                re_printf("tcp_send error\n");
                close_handler(0, client);
            }
        }

        client->bCandidatesSent = true;
        if (client->bCandidatesRcvd && client->bCandidatesSent) {
            client->state = ClientState::Completed;
        }
        tmr_start(&client->reTimer, CLIENT_TIMER_DELAY, timer_handler, client);
    } else if (client->state == ClientState::Completed) {
        close_handler(0, client);
    }

    mem_deref(mb);
}

static void signal_handler(int sig)
{
    re_printf("terminating on signal %d...\n", sig);
    re_cancel();
}

static void establish_handler(void *arg)
{
    SignalClient *client = (SignalClient *)arg;
    client->state = ClientState::Connected;

    tmr_start(&client->reTimer, CLIENT_TIMER_DELAY, timer_handler, client);
}

static void recv_handler(struct mbuf *mb, void *arg)
{
    struct sa peerCandidate;
    SignalClient *client = (SignalClient *)arg;

    re_printf("%zu - recv_handler: %zd bytes - %s\n", client->id, mbuf_get_left(mb), stringFromClientState(client->state));

    while (mbuf_get_left(mb)) {
        MessageID msgID = (MessageID)mbuf_read_u8(mb);
        uint8_t msgLen = mbuf_read_u8(mb);

        re_printf("%zu - recv_handler: msgLen: %u, msgId: %s\n", client->id, msgLen, stringFromMessageID(msgID));

        if (msgID == MessageID::ChannelReady) {
            client->state = ClientState::Exchanging;
            tmr_start(&client->reTimer, CLIENT_TIMER_DELAY, timer_handler, client);
        } else if (msgID == MessageID::PeerCandidates) {
            for (int i = 0; i < msgLen / 6; i++) {
                sa_init(&peerCandidate, AF_INET);
                peerCandidate.u.in.sin_addr.S_un.S_addr = mbuf_read_u32(mb);
                peerCandidate.u.in.sin_port = mbuf_read_u16(mb);

                re_printf("%zu - recv_handler: candidate: %J\n", client->id, &peerCandidate);
            }

            client->bCandidatesRcvd = true;
            if (client->bCandidatesRcvd && client->bCandidatesSent) {
                client->state = ClientState::Completed;
            }
            tmr_start(&client->reTimer, CLIENT_TIMER_DELAY, timer_handler, client);
        } else if (msgID == MessageID::Complete) {
            tmr_start(&client->reTimer, CLIENT_TIMER_DELAY, timer_handler, client);
        }
    }
}

static void close_handler(int err, void *arg)
{
    SignalClient *client = (SignalClient *)arg;
    re_printf("%zu - close_handler\n", client->id);

    tmr_cancel(&client->reTimer);
    mem_deref(client->tcpConn);

    g_clientsCompleted++;
    re_printf("%d clients completed\n", g_clientsCompleted);
    if (g_clientsCompleted >= CLIENT_COUNT) {
        re_cancel();
    }
}

static int launchIndex = 0;
static struct tmr launchTimer;
static struct sa serverAddress;
SignalClient client[CLIENT_COUNT] = {0};
void launch_timer_handler(void *arg)
{
    (void)arg;
    int err;
    char strnumber[40] = {0};
    itoa(CLIENT_COUNT + launchIndex, strnumber, 10);

    re_printf("launching: %d and %d\n", launchIndex, launchIndex + 1);

    client[launchIndex].id = 0;
    client[launchIndex + 1].id = 1;

    strcpy(client[launchIndex].channelIdString, "0000potatoes");
    for (int c = 0; c < strlen(strnumber); c++) {
        client[launchIndex].channelIdString[c] = strnumber[c];
    }
    strcpy(client[launchIndex + 1].channelIdString, client[launchIndex].channelIdString);

    err = tcp_connect(&client[launchIndex].tcpConn, &serverAddress, establish_handler, recv_handler, close_handler, &client[launchIndex]);
    if (err) {
        re_printf("Unable to connect client%d to: %J\n", launchIndex, &serverAddress);
        g_clientsCompleted++;
        re_printf("%d clients completed\n", g_clientsCompleted);
        if (g_clientsCompleted >= CLIENT_COUNT) {
            re_cancel();
        }
    }

    err = tcp_connect(&client[launchIndex + 1].tcpConn, &serverAddress, establish_handler, recv_handler, close_handler, &client[launchIndex + 1]);
    if (err) {
        re_printf("Unable to connect client%d to: %J\n", launchIndex + 1, &serverAddress);
        g_clientsCompleted++;
        re_printf("%d clients completed\n", g_clientsCompleted);
        if (g_clientsCompleted >= CLIENT_COUNT) {
            re_cancel();
        }
    }

    launchIndex += 2;
    if (launchIndex >= CLIENT_COUNT) {
        tmr_cancel(&launchTimer);
    } else {
        tmr_start(&launchTimer, CLIENT_LAUNCH_DELAY, launch_timer_handler, NULL);
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
    
    sa_set_str(&serverAddress, "127.0.0.1", 3456);

    tmr_start(&launchTimer, CLIENT_LAUNCH_DELAY, launch_timer_handler, NULL);
    
    /* main loop */
    err = re_main(signal_handler);

out:
    /* free library state */
    libre_close();

    /* check for memory leaks */
    tmr_debug();
    mem_debug();

    return err;
}
