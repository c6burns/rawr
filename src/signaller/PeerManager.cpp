// Copyright: TLM Partners Inc. - 2021

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "re.h"

#include "SignalChannel.h"
#include "SignalPeer.h"

std::map<SignalPeerId, std::shared_ptr<SignalPeer>> PeerManager::PeerMap;

std::map<std::string, std::shared_ptr<SignalChannel>> PeerManager::ChannelMap;

static uint64_t hashFromSockAddress(const struct sa *sockAddress)
{
    uint64_t rv = sa_port(sockAddress);
    rv <<= 32;
    rv += sa_in(sockAddress);

    return rv;
}

SignalPeer *PeerManager::CreatePeer(const struct sa *sockAddress)
{
    std::shared_ptr<SignalPeer> signalPeer(new SignalPeer());

    signalPeer->id = hashFromSockAddress(sockAddress);
    signalPeer->state = PeerState::NoChannel;
    signalPeer->reConn = NULL;
    tmr_init(&signalPeer->reTimer);
    sa_cpy(&signalPeer->reSA, sockAddress);

    PeerMap.insert(std::make_pair(signalPeer->id, signalPeer));

    return signalPeer.get();
}

SignalPeer *PeerManager::Peer(SignalPeerId peerId)
{
    auto it = PeerMap.find(peerId);
    if (it == PeerMap.end()) {
        return NULL;
    }
    return it->second.get();
}

size_t PeerManager::PeerCount()
{
    return PeerMap.size();
}

void PeerManager::DisconnectPeer(SignalPeerId peerId)
{
    SignalPeer *signalPeer = PeerManager::Peer(peerId);
    if (!signalPeer) {
        return;
    }

    SignalChannel *signalChannel = PeerManager::Channel(signalPeer->channelId);
    if (signalChannel) {
        signalChannel->peers.erase(signalPeer->id);
        if (signalChannel->peers.empty()) {
            PeerManager::CloseChannel(signalChannel->id);
        }
    }

    PeerMap.erase(peerId);
}

SignalChannel *PeerManager::CreateChannel(const char *channelIdString)
{
    if (!strlen(channelIdString)) {
        return NULL;
    }

    std::shared_ptr<SignalChannel> signalChannel(new SignalChannel());
    signalChannel->id = channelIdString;
    signalChannel->state = ChannelState::AwaitPeer;

    ChannelMap.insert(std::make_pair(signalChannel->id, signalChannel));

    return signalChannel.get();
}

SignalChannel *PeerManager::Channel(SignalChannelId channelId)
{
    auto it = ChannelMap.find(channelId);
    if (it == ChannelMap.end()) {
        return NULL;
    }
    return it->second.get();
}

size_t PeerManager::ChannelCount()
{
    return ChannelMap.size();
}

void PeerManager::CloseChannel(SignalChannelId channelId)
{
    auto it = ChannelMap.find(channelId);
    if (it == ChannelMap.end()) {
        return;
    }
    ChannelMap.erase(channelId);
}

void PeerManager::Shutdown()
{
    std::vector<SignalPeerId> peerIdList(PeerMap.size());
    for (auto pair : PeerMap) {
        peerIdList.push_back(pair.first);
    }

    for (SignalPeerId peerId : peerIdList) {
        PeerManager::DisconnectPeer(peerId);
    }
    PeerMap.clear();

    ChannelMap.clear();
}
