// Copyright: TLM Partners Inc. - 2021

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

class SignalPeer;
class SignalChannel;

struct sa;
struct tcp_conn;

typedef uint64_t SignalPeerId;
typedef std::string SignalChannelId;

/**
 * Responsible for constructing SignalPeers and SignalChannels correctly, and assigning a
 * pointer back to this factory / manager class
 */
class PeerManager
{
  public:
    static SignalPeer *CreatePeer(const struct sa *sockAddress);
    static SignalPeer *Peer(SignalPeerId peerId);
    static size_t PeerCount();
    static void DisconnectPeer(SignalPeerId peerId);

    static SignalChannel *CreateChannel(const char *channelIdString);
    static SignalChannel *Channel(SignalChannelId channelId);
    static size_t ChannelCount();
    static void CloseChannel(SignalChannelId channelId);

    static void Shutdown();

  protected:
    static std::map<SignalPeerId, std::shared_ptr<SignalPeer>> PeerMap;
    static std::map<std::string, std::shared_ptr<SignalChannel>> ChannelMap;
};
