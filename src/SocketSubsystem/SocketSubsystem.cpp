// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystem.h"
#include "Socket.h"

// -----------------------------------------------------------------------------------------------------------------------------
void ISocketSubsystem::ShutdownAllSystems()
{

}

// -----------------------------------------------------------------------------------------------------------------------------
bool ISocketSubsystem::Init()
{
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------
void ISocketSubsystem::Shutdown()
{

}

// -----------------------------------------------------------------------------------------------------------------------------
FSocket *ISocketSubsystem::CreateSocket(int ue4CompatName, const char *ue4CompatString, FNetworkProtocolTypes ue4CompatType)
{
    int64 fd = 0;
    if (ue4CompatName == NAME_DGram) {
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    } else if (ue4CompatName == NAME_Stream) {
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    } else {
        return nullptr;
    }

    return new FSocket(fd);
}


// -----------------------------------------------------------------------------------------------------------------------------
void ISocketSubsystem::DestroySocket(FSocket* Socket)
{

}

// -----------------------------------------------------------------------------------------------------------------------------
TSharedRef<FInternetAddr> ISocketSubsystem::CreateInternetAddr()
{
    return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------
bool ISocketSubsystem::HasNetworkDevice()
{
    return false;
}

// -----------------------------------------------------------------------------------------------------------------------------
const TCHAR* ISocketSubsystem::GetSocketAPIName() const
{
    return "C6";
}

// -----------------------------------------------------------------------------------------------------------------------------
bool ISocketSubsystem::GetLocalAdapterAddresses(TArray<TSharedRef<FInternetAddr>>& OutAddresses)
{
    return false;
}

// -----------------------------------------------------------------------------------------------------------------------------
int32 ISocketSubsystem::BindNextPort(FSocket* Socket, FInternetAddr& Addr, int32 PortCount, int32 PortIncrement)
{
    return 0;
}
