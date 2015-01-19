#include "ktunnel.h"

extern struct neigh_table arp_tbl;

static int (*_txFn)(void* data, int dataLen) = NULL;

static u32                _dstip     = 0;
static bool               _txEnabled = false;
static struct sockaddr_in _txaddr    = {};
static struct socket*     _txsock    = NULL;

static int _netpollSend(void* data, int dataLen)
{
    int               retval     = 0;
    struct rtable*    routingTbl = NULL;
    struct flowi4     fl4        = {};
    struct neighbour* neigh      = NULL;
    struct in_device* indev      = NULL;
    struct netpoll    np         = {};

    CHECK_IF(NULL == data, goto _err_return, "data is null");
    CHECK_IF(0 >= dataLen, goto _err_return, "dataLen <= 0");

    fl4.flowi4_oif = 0;
    fl4.flowi4_tos = 0;
    fl4.saddr      = 0;
    fl4.daddr      = _dstip;

    routingTbl = ip_route_output_key(&init_net, &fl4);
    CHECK_IF(NULL == routingTbl, goto _err_return, "get no routing table for dstip = %pI4", &_dstip);

    neigh = neigh_lookup(&arp_tbl, &(fl4.daddr), routingTbl->dst.dev);
    CHECK_IF(NULL == neigh, goto _err_return, "find no arp info by dstip = %pI4", &_dstip);

    // get in_net from outgoing net_device & get source ip address from in_net
    indev = __in_dev_get_rtnl(routingTbl->dst.dev);
    CHECK_IF(NULL == indev, goto _err_return, "get no source ip addr by in_net");

    // fill netpoll info
    strlcpy(np.dev_name, routingTbl->dst.dev->name, IFNAMSIZ);
    np.local_ip.ip  = indev->ifa_list->ifa_local;
    np.remote_ip.ip = fl4.daddr;
    np.local_port   = g_tunnelPort;
    np.remote_port  = g_tunnelPort;
    memcpy(np.remote_mac, neigh->ha, ETH_ALEN);

    // fill the rest field of netpoll info
    retval = netpoll_setup(&np);
    CHECK_IF(0 != retval, goto _err_return, "nepoll setup failed");

    // netpoll_send_udp return void, so we suppose it works.
    netpoll_send_udp(&np, data, dataLen);

    return dataLen;

_err_return:
    return -1;
}

static void _uninitNetpollTx(void)
{
    // do nothing
    return;
}

static int _initNetpollTx(void)
{
    // do nothing
    return 0;
}

static int _udpSend(void* data, int dataLen)
{
    struct msghdr msg = {};
    struct iovec  iov = {};

    CHECK_IF(NULL == data, goto _err_return, "data is null");
    CHECK_IF(0 >= dataLen, goto _err_return, "dataLen <= 0");

    iov.iov_base       = data;
    iov.iov_len        = dataLen;

    msg.msg_flags      = 0;
    msg.msg_name       = &(_txaddr);
    msg.msg_namelen    = sizeof(struct sockaddr_in);
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;

    return sock_sendmsg(_txsock, &msg, dataLen);

_err_return:
    return -1;
}

static void _uninitUdpTx(void)
{
    if (_txsock)
    {
        sock_release(_txsock);
        _txsock = NULL;
    }
    return;
}

static int _initUdpTx(void)
{
    _txsock = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == _txsock,      goto _err_return, "my socket failed");
    CHECK_IF(NULL == _txsock->ops, goto _err_return, "my socket failed");

    _txaddr.sin_family      = AF_INET;
    _txaddr.sin_addr.s_addr = _dstip;
    _txaddr.sin_port        = htons(g_tunnelPort);

    return 0;

_err_return:
    if (_txsock)
    {
        sock_release(_txsock);
        _txsock = NULL;
    }
    return -1;
}

int ktunnel_send(void* data, int dataLen)
{
    if (_txFn)
    {
        return _txFn(data, dataLen);
    }

    derror("_txFn is null");
    return -1;
}

int ktunnel_initTx(char* txmode)
{
    int retval = 0;

    CHECK_IF(NULL == txmode, return -1, "txmode is null");

    retval = my_inet_pton(AF_INET, g_dstRealip, &_dstip);
    CHECK_IF(0 > retval, return -1, "my inet pton failed");

    if (0 == strcmp(txmode, "udp"))
    {
        retval = _initUdpTx();
        _txFn = _udpSend;
    }
    else if (0 == strcmp(txmode, "netpoll"))
    {
        retval = _initNetpollTx();
        _txFn = _netpollSend;
    }
    else
    {
        derror("unknown txmode = %s", txmode);
        return -1;
    }

    _txEnabled = true;

    return retval;
}

void ktunnel_uninitTx(char* txmode)
{
    CHECK_IF(NULL == txmode, return, "txmode is null");

    if (!_txEnabled) { return; }

    if (0 == strcmp(txmode, "udp"))
    {
        _uninitUdpTx();
    }
    else if (0 == strcmp(txmode, "netpoll"))
    {
        _uninitNetpollTx();
    }
    else
    {
        derror("unknown txmode = %s", txmode);
    }

    _txFn      = NULL;
    _txEnabled = false;

    return;
}
