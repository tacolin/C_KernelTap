//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
extern struct neigh_table arp_tbl;

//////////////////////////////////////////////////////////////////////////////
//
//      Functions
//
//////////////////////////////////////////////////////////////////////////////
bool knetpoll_getInfo(char* dstip, struct netpoll* np)
{
    struct rtable*    routingTbl = NULL;
    struct flowi4     fl4        = {};
    struct neighbour* neigh      = NULL;
    struct in_device* indev      = NULL;

    int ret = 0;

    if (NULL == dstip)
    {
        dprint("dst ip is null");
        goto _ERROR;
    }

    if (NULL == np)
    {
        dprint("np is null");
        goto _ERROR;
    }

    fl4.flowi4_oif = 0;
    fl4.flowi4_tos = 0;
    fl4.saddr      = 0;
    ret = my_inet_pton(AF_INET, dstip, &(fl4.daddr));
    if (0 > ret)
    {
        dprint("my inet pton dst failed");
        goto _ERROR;
    }

    // get routing table
    // get outgoing net_device from routing table
    routingTbl = ip_route_output_key(&init_net, &fl4);
    if ((NULL == routingTbl) || IS_ERR(routingTbl))
    {
        dprint("find no routing table with dstip = %s", dstip);
        goto _ERROR;
    }

    if (NULL == routingTbl->dst.dev)
    {
        dprint("find no dst device with dstip = %s", dstip);
        goto _ERROR;
    }

    // get dstination mac address from arp table
    neigh = neigh_lookup(&arp_tbl, &(fl4.daddr), routingTbl->dst.dev);
    if (NULL == neigh)
    {
        // dprint("find no neighbour in arp table");
        goto _ERROR;
    }

    // get in_net from outgoing net_device
    // get source ip address from in_net
    indev = __in_dev_get_rtnl(routingTbl->dst.dev);
    if (NULL == indev)
    {
        dprint("in_dev get failed");
        goto _ERROR;
    }

    strlcpy(np->dev_name, routingTbl->dst.dev->name, IFNAMSIZ);

    np->local_ip.ip  = indev->ifa_list->ifa_local;
    np->remote_ip.ip = fl4.daddr;
    np->local_port   = g_tunnelPort;
    np->remote_port  = g_tunnelPort;

    memcpy(np->remote_mac, neigh->ha, ETH_ALEN);

    ret = netpoll_setup(np);
    if (0 != ret)
    {
        dprint("netpoll setup failed");
        goto _ERROR;
    }

    return true;

_ERROR:
    return false;
}

int knetpoll_send(struct netpoll *np, void* data, int dataLen)
{
    if (NULL == np)
    {
        dprint("np is null");
        return -1;
    }

    if (NULL == data)
    {
        dprint("data is null");
        return -1;
    }

    if (0 >= dataLen)
    {
        dprint("dataLen = %d <= 0", dataLen);
        return -1;
    }

    netpoll_send_udp(np, data, dataLen);

    return dataLen;
}
