#pragma once

#include "Types.h"

#include "Socket.h"

#define PLATFORM_SOCKETSUBSYSTEM 1

class FInternetAddr;

class ISocketSubsystem
{
private:
    ISocketSubsystem() {}

public:
    static ISocketSubsystem *Get(int ue4CompatibilityInt)
    {
        return ISocketSubsystem::Get();
    }

	static ISocketSubsystem* Get()
	{
		static ISocketSubsystem instance;
		return &instance;
	}

	ISocketSubsystem(ISocketSubsystem const&) = delete;
    void operator=(ISocketSubsystem const&) = delete;

	/**
	 * Shutdown all registered subsystems
	 */
	static void ShutdownAllSystems();


	virtual ~ISocketSubsystem() { }

	/**
	 * Does per platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return true if initialized ok, false otherwise
	 */
	virtual bool Init();

	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Shutdown();

	/**
	 * Creates a socket
	 *
	 * @Param ue4CompatName - SocketType type of socket to create (DGram, Stream, etc)
	 * @param ue4CompatString - SocketDescription debug description
	 * @param ue4CompatType - select between IPv4 or IPv6
	 *
	 * @return the new socket or NULL if failed
	 */
    virtual FSocket *CreateSocket(int ue4CompatName, const char *ue4CompatString, FNetworkProtocolTypes ue4CompatType);

	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(FSocket* Socket);

	/**
	 * Create a proper FInternetAddr representation
	 */
	virtual TSharedRef<FInternetAddr> CreateInternetAddr();

	/**
	 * @return Whether the machine has a properly configured network device or not
	 */
	virtual bool HasNetworkDevice();

	/**
	 *	Get the name of the socket subsystem
	 * @return a string naming this subsystem
	 */
	virtual const TCHAR* GetSocketAPIName() const;

	/**
	 * Gets the list of addresses associated with the adapters on the local computer.
	 * Unlike GetLocalHostAddr, this function does not give preferential treatment to multihome options in results.
	 * It's encouraged that users check for multihome before using the results of this function.
	 *
	 * @param OutAddresses - Will hold the address list.
	 *
	 * @return true on success, false otherwise.
	 */
	virtual bool GetLocalAdapterAddresses(TArray<TSharedRef<FInternetAddr>>& OutAddresses);

	/**
	 * Bind to next available port.
	 *
	 * @param Socket The socket that that will bind to the port
	 * @param Addr The local address and port that is being bound to (usually the result of GetLocalBindAddr()). This addresses port number will be modified in place
	 * @param PortCount How many ports to try
	 * @param PortIncrement The amount to increase the port number by on each attempt
	 *
	 * @return The bound port number, or 0 on failure
	 */
	int32 BindNextPort(FSocket* Socket, FInternetAddr& Addr, int32 PortCount, int32 PortIncrement);
};
