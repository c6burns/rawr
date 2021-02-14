// Copyright: TLM Partners Inc. - 2021

#pragma once

#include <memory>
#include <set>
#include <string>

#include "re.h"

#include "PeerManager.h"

enum class ChannelState : uint8_t {
    None,
    AwaitPeer,
    Exchange,
    Complete,
};

/**
 * Represents a single SignalChannel instance. These are not meant to be 
 * created directly, but constructed using PeerManager::CreateSignalChannel
 */
class SignalChannel
{
  public:
    std::string id;
    ChannelState state;
    std::set<SignalPeerId> peers;
};
