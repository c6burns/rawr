#include "InternetAddr.h"

#include <cstring>

// -----------------------------------------------------------------------------------------------------------------------------
FInternetAddr::FInternetAddr()
{
    memset(&m_sockaddr, 0, sizeof(m_sockaddr));
    m_sockaddr.sin_family = AF_INET;
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetIp(uint32 InAddr)
{
    m_sockaddr.sin_addr.s_addr = htonl(InAddr);
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetIp(const char* InAddr, bool& bIsValid)
{
    int err;
    bIsValid = false;

    err = inet_pton(AF_INET, InAddr, &m_sockaddr.sin_addr.s_addr);
    if (err) return;

    bIsValid = true;
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::GetIp(uint32& OutAddr) const
{
    OutAddr = ntohl(m_sockaddr.sin_addr.s_addr);
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetPort(int32 InPort)
{
    m_sockaddr.sin_port = htons((uint16)InPort);
}

// -----------------------------------------------------------------------------------------------------------------------------
int32 FInternetAddr::GetPort() const
{
    return ntohs(m_sockaddr.sin_port);
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetAnyAddress()
{
    m_sockaddr.sin_addr.s_addr = INADDR_ANY;
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetBroadcastAddress()
{
    m_sockaddr.sin_addr.s_addr = INADDR_BROADCAST;
}

// -----------------------------------------------------------------------------------------------------------------------------
void FInternetAddr::SetLoopbackAddress()
{
    m_sockaddr.sin_addr.s_addr = INADDR_LOOPBACK;
}

// -----------------------------------------------------------------------------------------------------------------------------
bool FInternetAddr::IsValid() const
{
    return true;
}
