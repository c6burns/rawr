#pragma once

#if _WIN32
#    include <WinSock2.h>
#    include <ws2ipdef.h>
#    include <ws2tcpip.h>
#else
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <netinet/udp.h>
#    include <sys/socket.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#endif

#include <cstdint>
#include <vector>
#include <memory>

typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;
typedef char TCHAR;

#define FString std::string
#define TArray std::vector
#define TSharedRef std::shared_ptr

enum class ESocketProtocolFamily : uint8
{
	/** No protocol family specification. Typically defined as AF_UNSPEC */
	None,
	/** IPv4 and IPv6 respectively. */
	IPv4,
	IPv6
};

enum ESocketType
{
	/** Not bound to a protocol yet */
	SOCKTYPE_Unknown,
	/** A UDP type socket */
	SOCKTYPE_Datagram,
	/** A TCP type socket */
	SOCKTYPE_Streaming
};

enum ESocketConnectionState
{
	SCS_NotConnected,
	SCS_Connected,
	/** Indicates that the end point refused the connection or couldn't be reached */
	SCS_ConnectionError
};

namespace ESocketReceiveFlags
{
	/**
	 * Enumerates socket receive flags.
	 */
	enum Type
	{
		/**
		 * Return as much data as is currently available in the input queue,
		 * up to the specified size of the receive buffer.
		 */
		None = 0,

		/**
		 * Copy received data into the buffer without removing it from the input queue.
		 */
		 Peek = 2,

		 /**
		  * Block the receive call until either the supplied buffer is full, the connection
		  * has been closed, the request has been canceled, or an error occurred.
		  */
		  WaitAll = 0x100
	};
}


namespace ESocketWaitConditions
{
	/**
	 * Enumerates socket wait conditions.
	 */
	enum Type
	{
		/**
		 * Wait until data is available for reading.
		 */
		WaitForRead,

		/**
		 * Wait until data can be written.
		 */
		 WaitForWrite,

		 /**
		  * Wait until data is available for reading or can be written.
		  */
		  WaitForReadOrWrite
	};
}

/**
 * Enumerates socket shutdown modes.
 */
enum class ESocketShutdownMode
{
	/**
	 * Disables reading on the socket.
	 */
	Read,

	/**
	 * Disables writing on the socket.
	 */
	 Write,

	 /**
	  * Disables reading and writing on the socket.
	  */
	  ReadWrite
};
