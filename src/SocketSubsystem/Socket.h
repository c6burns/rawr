#pragma once

#include "Types.h"

class FInternetAddr;

enum class FNetworkProtocolTypes {
    None,
    IPv4,
};

#define NAME_DGram ESocketType::SOCKTYPE_Datagram
#define NAME_Stream ESocketType::SOCKTYPE_Streaming

/**
 * This is our abstract base class that hides the platform specific socket implementation
 */
class FSocket
{
protected:
    int64_t fd;

public:

	/** Default constructor. */
	FSocket(int64_t sockFd) :
		fd(sockFd)
	{ }

	/** Virtual destructor. */
	virtual ~FSocket()
	{ }

	/**
	 * Shuts down the socket, making it unusable for reads and/or writes. This does not close the socket!
	 *
	 * @param Types
	 * @return true if successful, false otherwise.
	 */
	virtual bool Shutdown(ESocketShutdownMode Mode);

	/**
	 * Closes the socket
	 *
	 * @return true if it closes without errors, false otherwise.
	 */
	virtual bool Close();

	/**
	 * Binds a socket to a network byte ordered address.
	 *
	 * @param Addr The address to bind to.
	 * @return true if successful, false otherwise.
	 */
	virtual bool Bind(const FInternetAddr& Addr);

	/**
	 * Connects a socket to a network byte ordered address.
	 *
	 * @param Addr The address to connect to.
	 * @return true if successful, false otherwise.
	 */
	virtual bool Connect(const FInternetAddr& Addr);

	/**
	 * Places the socket into a state to listen for incoming connections.
	 *
	 * @param MaxBacklog The number of connections to queue before refusing them.
	 * @return true if successful, false otherwise.
	 */
	virtual bool Listen(int32 MaxBacklog);

	/**
	 * Accepts a connection that is pending.
	 *
	 * @param SocketDescription Debug description of socket,
	 * @return The new (heap-allocated) socket, or nullptr if unsuccessful.
	 */
	virtual class FSocket* Accept();

	/**
	 * Accepts a connection that is pending.
	 *
	 * @param OutAddr The address of the connection.
	 * @param SocketDescription Debug description of socket.
	 * @return The new (heap-allocated) socket, or nullptr if unsuccessful.
	 */
	virtual class FSocket* Accept(FInternetAddr& OutAddr);

	/**
	 * Sends a buffer to a network byte ordered address.
	 *
	 * @param Data The buffer to send.
	 * @param Count The size of the data to send.
	 * @param BytesSent Will indicate how much was sent.
	 * @param Destination The network byte ordered address to send to.
	 */
	virtual bool SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination);

	/**
	 * Sends a buffer on a connected socket.
	 *
	 * @param Data The buffer to send.
	 * @param Count The size of the data to send.
	 * @param BytesSent Will indicate how much was sent.
	 */
	virtual bool Send(const uint8* Data, int32 Count, int32& BytesSent);

	/**
	 * Reads a chunk of data from the socket and gathers the source address.
	 *
	 * A return value of 'true' does not necessarily mean that data was returned.
	 * Callers must check the 'BytesRead' parameter for the actual amount of data
	 * returned. A value of zero indicates that there was no data available for reading.
	 *
	 * @param Data The buffer to read into.
	 * @param BufferSize The max size of the buffer.
	 * @param BytesRead Will indicate how many bytes were read from the socket.
	 * @param Source Will contain the receiving the address of the sender of the data.
	 * @param Flags The receive flags.
	 * @return true on success, false in case of a closed socket or an unrecoverable error.
	 */
	virtual bool RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None);

	/**
	 * Reads a chunk of data from a connected socket
	 *
	 * A return value of 'true' does not necessarily mean that data was returned.
	 * Callers must check the 'BytesRead' parameter for the actual amount of data
	 * returned. A value of zero indicates that there was no data available for reading.
	 *
	 * @param Data The buffer to read into
	 * @param BufferSize The max size of the buffer
	 * @param BytesRead Will indicate how many bytes were read from the socket
	 * @param Flags the receive flags
	 * @return true on success, false in case of a closed socket or an unrecoverable error.
	 */
	virtual bool Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags = ESocketReceiveFlags::None);

	/**
	 * Determines the connection state of the socket.
	 *
	 * @return Connection state.
	 */
	virtual ESocketConnectionState GetConnectionState();

	/**
	 * Reads the address the socket is bound to and returns it.
	 * 
	 * @param OutAddr The address the socket is bound to.
	 */
	virtual void GetAddress(FInternetAddr& OutAddr);

	/**
	 * Reads the address of the peer the socket is connected to.
	 *
	 * @param OutAddr Address of the peer the socket is connected to.
	 * @return true if the address was retrieved correctly, false otherwise.
	 */
	virtual bool GetPeerAddress(FInternetAddr& OutAddr);

	/**
	 * Sets this socket into non-blocking mode.
	 *
	 * @param bIsNonBlocking Whether to enable blocking or not.
	 * @return true if successful, false otherwise.
	 */
	virtual bool SetNonBlocking(bool bIsNonBlocking = true);

	/**
	 * Sets a socket into broadcast mode (UDP only).
	 *
	 * @param bAllowBroadcast Whether to enable broadcast or not.
	 * @return true if successful, false otherwise.
	 */
	virtual bool SetBroadcast(bool bAllowBroadcast = true);

	/**
	 * Sets this socket into TCP_NODELAY mode (TCP only).
	 *
	 * @param bIsNoDelay Whether to enable no delay mode.
	 * @return true if successful, false otherwise.
	 */
	virtual bool SetNoDelay(bool bIsNoDelay = true);

	/**
	 * Joins this socket to the specified multicast group.
	 *
	 * The multicast group address must be in the range 224.0.0.0 to 239.255.255.255.
	 *
	 * @param GroupAddress The IP address of the multicast group.
	 * @return true on success, false otherwise.
	 * @see LeaveMulticastGroup, SetMulticastLoopback, SetMulticastTtl
	 */
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress);

	/**
	 * Joins this socket to the specified multicast group on the specified interface.
	 *
	 * The multicast group address must be in the range 224.0.0.0 to 239.255.255.255.
	 *
	 * @param GroupAddress The IP address of the multicast group.
	 * @param InterfaceAddress The address representing the interface.
	 * @return true on success, false otherwise.
	 * @see LeaveMulticastGroup, SetMulticastLoopback, SetMulticastTtl
	 */
	virtual bool JoinMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress);

	/**
	 * Removes this UDP client from the specified multicast group.
	 *
	 * @param The multicast group address to leave.
	 * @return true on success, false otherwise.
	 * @see JoinMulticastGroup, SetMulticastLoopback, SetMulticastTtl
	 */
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress);

	/**
	 * Removes this UDP client from the specified multicast group on the specified interface.
	 *
	 * @param GroupAddress The multicast group address to leave.
	 * @param InterfaceAddress The address representing the interface.
	 * @return true on success, false otherwise.
	 * @see JoinMulticastGroup, SetMulticastLoopback, SetMulticastTtl
	 */
	virtual bool LeaveMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress);

	/**
	 * Enables or disables multicast loopback on the socket (UDP only).
	 *
	 * This setting determines whether multicast datagrams are looped
	 * back to the sending socket. By default, multicast loopback is
	 * enabled. It must be enabled if more than one listener is present
	 * on a host.
	 *
	 * @param bLoopback Whether loopback should be enabled.
	 * @see LeaveMulticastGroup, JoinMulticastGroup, SetMulticastTtl
	 */
	virtual bool SetMulticastLoopback(bool bLoopback);

	/**
	 * Sets the time to live (TTL) for multicast datagrams.
	 *
	 * The default TTL for multicast datagrams is 1, which prevents them
	 * from being forwarded beyond the local subnet. Higher values will
	 * allow multicast datagrams to be sent into neighboring subnets, if
	 * multicast capable routers are present.
	 *
	 * @param TimeToLive Number of hops the datagram can make.
	 * @see LeaveMulticastGroup, JoinMulticastGroup, SetMulticastLoopback
	 */
	virtual bool SetMulticastTtl(uint8 TimeToLive);

	/**
	 * Sets the interface used to send outgoing multicast datagrams.
	 *
	 * Multicast traffic is sent using the default interface, this allows 
	 * to explicitly set the interface used to send outgoing multicast datagrams.
	 *
	 * @param InterfaceAddress The interface address.
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetMulticastInterface(const FInternetAddr& InterfaceAddress);

	/**
	 * Sets whether a socket can be bound to an address in use.
	 *
	 * @param bAllowReuse Whether to allow reuse or not.
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetReuseAddr(bool bAllowReuse = true);

	/**
	 * Sets whether and how long a socket will linger after closing.
	 *
	 * @param bShouldLinger Whether to have the socket remain open for a time period after closing or not.
	 * @param Timeout The amount of time to linger before closing.
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetLinger(bool bShouldLinger = true, int32 Timeout = 0);

	/**
	 * Enables error queue support for the socket.
	 *
	 * @param bUseErrorQueue Whether to enable error queuing or not.
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetRecvErr(bool bUseErrorQueue = true);

	/**
	 * Sets the size of the send buffer to use.
	 *
	 * @param Size The size to change it to.
	 * @param NewSize Will contain the size that was set (in case OS can't set that).
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetSendBufferSize(int32 Size, int32& NewSize);

	/**
	 * Sets the size of the receive buffer to use.
	 *
	 * @param Size The size to change it to.
	 * @param NewSize Will contain the size that was set (in case OS can't set that).
	 * @return true if the call succeeded, false otherwise.
	 */
	virtual bool SetReceiveBufferSize(int32 Size, int32& NewSize);

	/**
	 * Sets whether to retrieve the system-level receive timestamp, for sockets
	 *
	 * @param bRetrieveTimestamp	Whether to retrieve the timestamp upon receive
	 * @return						True if the call succeeded, false otherwise.
	 */
	virtual bool SetRetrieveTimestamp(bool bRetrieveTimestamp=true);

	/**
	 * Reads the port this socket is bound to.
	 *
	 * @return Port number.
	 * @see GetSocketType, GetDescription
	 */ 
	virtual int32 GetPortNo();

};
