// Copyright: TLM Partners Inc. - 2021

#pragma once

#include "SignalPeer.h"

#include "PeerManager.h"

struct sa;

class SignalChannel;

enum class PeerState : uint8_t {
    None,
    NoChannel,
    Valid,
    SentCandidates,
};

class SignalPeer
{
  public:
    SignalPeer()
        : id(0), state(PeerState::None), channelId("")
    {
        id = 0;
        state = PeerState::None;
        reConn = NULL;
        sa_init(&reSA, AF_INET);
        tmr_init(&reTimer);
    }

    ~SignalPeer()
    {
        mem_deref(reConn);
        tmr_cancel(&reTimer);
    }

    uint64_t id;
    PeerState state;
    SignalChannelId channelId;

    struct tcp_conn *reConn;
    struct sa reSA;
    struct tmr reTimer;
};
