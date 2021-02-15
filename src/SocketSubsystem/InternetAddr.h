#pragma once

#include "Types.h"

/**
 * Represents an internet address. All data is in network byte order
 */
class FInternetAddr
{
protected:
    struct sockaddr_in m_sockaddr;

	/** Hidden on purpose */
    FInternetAddr();

public:

	virtual ~FInternetAddr() {}

	/**
	 * Sets the ip address from a host byte order uint32
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr);
	
	/**
	 * Sets the ip address from a string ("A.B.C.D")
	 *
	 * @param InAddr the string containing the new ip address to use
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid);

	/**
	 * Copies the network byte order ip address to a host byte order dword
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const;

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort);

	/**
	 * Copies the port number from this address and places it into a host byte order int
	 *
	 * @param OutPort the host byte order int that receives the port
	 */
	virtual void GetPort(int32& OutPort) const
	{
		OutPort = GetPort();
	}

	/**
	 * Returns the port number from this address in host byte order
	 */
	virtual int32 GetPort() const;

	/** Sets the address to be any address */
	virtual void SetAnyAddress();

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress();

	/** Sets the address to loopback */
	virtual void SetLoopbackAddress();

	/**
	 * Converts this internet ip address to string form
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	//virtual FString ToString(bool bAppendPort) const;

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const
	{
		return false;
		//TArray<uint8> ThisIP = GetRawIp();
		//TArray<uint8> OtherIP = Other.GetRawIp();
		//return ThisIP == OtherIP && GetPort() == Other.GetPort();
	}

	/**
	 * Hash function for use with TMap's - exposed through FInternetAddrMapRef
	 *
	 * @return	The value to use for the hash
	 */
	virtual uint32 GetTypeHash() const;

	/**
	 * Is this a well formed internet address
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const;
	
	/**
	 * Creates a new structure with the same data as this structure
	 *
	 * @return The new structure
	 */
	//virtual TSharedRef<FInternetAddr> Clone() const;

	/**
	 * Returns the protocol type name of the address data currently stored in this struct
	 *
	 * @return The type of the address. 
	 *         If it's not known or overridden in a derived class, the return is None.
	 */
	//virtual FName GetProtocolType() const
	//{
	//	return NAME_None;
	//}
};
