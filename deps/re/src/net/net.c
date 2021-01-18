/**
 * @file net.c  Networking code.
 *
 * Copyright (C) 2010 Creytiv.com
 */
#define _BSD_SOURCE 1
#define _DEFAULT_SOURCE 1
#include <stdlib.h>
#include <string.h>
#if defined(WIN32)
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    include <iphlpapi.h>
#else
#    define __USE_BSD 1 /**< Use BSD code */
#    include <unistd.h>
#    ifdef PS5
#        include <net.h>
#        include <libnetctl.h>
#    else
#        include <netdb.h>
#    endif
#endif
#include <re_fmt.h>
#include <re_mbuf.h>
#include <re_net.h>
#include <re_sa.h>
#include <re_types.h>

#define DEBUG_MODULE "net"
#define DEBUG_LEVEL 5
#include <re_dbg.h>

/**
 * Get the IP address of the host
 *
 * @param af  Address Family
 * @param ip  Returned IP address
 *
 * @return 0 if success, otherwise errorcode
 */
int net_hostaddr(int af, struct sa *ip)
{
#ifdef PS5
    int ret;
    SceNetCtlInfo info;
    SceNetInAddr inaddr = {0};

    if ((ret = sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &info)) < 0) {
        return -1;
    }

    if (inet_pton(AF_INET, info.ip_address, &ip->u.in.sin_addr) <= 0) {
        return -1;
    }
#else
    char hostname[256];
    char ipstr[32] = {0};
    struct in_addr in;
    struct hostent *he;

    if (-1 == gethostname(hostname, sizeof(hostname)))
        return errno;

    he = gethostbyname(hostname);
    if (!he)
        return ENOENT;

    if (af != he->h_addrtype)
        return EAFNOSUPPORT;

    /* Get the last entry */
    memcpy(&in, he->h_addr_list[0], sizeof(in));
    sa_set_in(ip, ntohl(in.s_addr), 0);
#endif
    return 0;
}

/**
 * Get the default source IP address
 *
 * @param af  Address Family
 * @param ip  Returned IP address
 *
 * @return 0 if success, otherwise errorcode
 */
int net_default_source_addr_get(int af, struct sa *ip)
{
    sa_init(ip, AF_INET);

#if defined(PS5)
    return net_hostaddr(af, ip);
#endif
#if defined(WIN32)

    PMIB_IPADDRTABLE pIPAddrTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    DWORD dwIndex = -1;

    if (GetBestInterface(0x80808080, &dwIndex) != NO_ERROR)
        goto cleanup;

    pIPAddrTable = (MIB_IPADDRTABLE *)malloc(sizeof(MIB_IPADDRTABLE));
    if (pIPAddrTable) {
        if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
            free(pIPAddrTable);
            pIPAddrTable = (MIB_IPADDRTABLE *)malloc(dwSize);
        }
        if (pIPAddrTable == NULL)
            goto cleanup;
    }
    if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) != NO_ERROR)
        goto cleanup;

    for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++) {
        if (pIPAddrTable->table[i].dwIndex != dwIndex) continue;
        ip->u.in.sin_addr.S_un.S_addr = pIPAddrTable->table[i].dwAddr;
        break;
    }

    free(pIPAddrTable);
    return 0;

cleanup:
    if (pIPAddrTable) free(pIPAddrTable);
    return -1;

#else
    char ifname[64] = "";

#    ifdef HAVE_ROUTE_LIST
    /* Get interface with default route */
    (void)net_rt_default_get(af, ifname, sizeof(ifname));
#    endif

    /* First try with default interface */
    if (0 == net_if_getaddr(ifname, af, ip))
        return 0;

    /* Then try first real IP */
    if (0 == net_if_getaddr(NULL, af, ip))
        return 0;

#    if PS5
    return -1;
#    else
    return net_if_getaddr4(ifname, af, ip);
#    endif
#endif
}

/**
 * Get a list of all network interfaces including name and IP address.
 * Both IPv4 and IPv6 are supported
 *
 * @param ifh Interface handler, called once per network interface
 * @param arg Handler argument
 *
 * @return 0 if success, otherwise errorcode
 */
int net_if_apply(net_ifaddr_h *ifh, void *arg)
{
#ifdef HAVE_GETIFADDRS
    return net_getifaddrs(ifh, arg);
#else
#    if PS5
    return -1;
#    else
    return net_if_list(ifh, arg);
#    endif
#endif
}

static bool net_rt_handler(const char *ifname, const struct sa *dst, int dstlen, const struct sa *gw, void *arg)
{
    void **argv = arg;
    struct sa *ip = argv[1];
    (void)dst;
    (void)dstlen;

    if (0 == str_cmp(ifname, argv[0])) {
        *ip = *gw;
        return true;
    }

    return false;
}

/**
 * Get the IP-address of the default gateway
 *
 * @param af  Address Family
 * @param gw  Returned Gateway address
 *
 * @return 0 if success, otherwise errorcode
 */
int net_default_gateway_get(int af, struct sa *gw)
{
    char ifname[64];
    void *argv[2];
    int err;

    if (!af || !gw)
        return EINVAL;

    err = net_rt_default_get(af, ifname, sizeof(ifname));
    if (err)
        return err;

    argv[0] = ifname;
    argv[1] = gw;

    err = net_rt_list(net_rt_handler, argv);
    if (err)
        return err;

    return 0;
}
