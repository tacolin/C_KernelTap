#include "ktunnel.h"

typedef int (*my_rxHandleFn)(void* data, int dataLen);

static my_rxHandleFn _rxHandleFn            = NULL;
static unsigned char _rxBuffer[BUFFER_SIZE] = {0};

static bool                _rxEnabled = false;
static struct sockaddr_in  _rxaddr    = {};
static struct socket*      _rxsock    = NULL;
static struct task_struct* _rxThread  = NULL;

static struct nf_hook_ops _hookOps   = {};

static bool _isKtunnelData(struct sk_buff *skb)
{
    struct iphdr*  iph  = NULL;
    struct udphdr* udph = NULL;
    u16 dstport         = 0;

    if (NULL == skb) { return false; }

    iph = ip_hdr(skb);
    if (NULL == iph) { return false; }

    if (iph->protocol != IPPROTO_UDP) { return false; }

    udph = udp_hdr(skb);
    if (NULL == udph) { return false; }

    dstport = ntohs(udph->dest);
    if (dstport != g_tunnelPort) { return false; }

    return true;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)

static unsigned int _netfilterRecv(const struct nf_hook_ops *ops,
                                   struct sk_buff *skb,
                                   const struct net_device *in,
                                   const struct net_device *out,
                                   int (*okfn)(struct sk_buff*))

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)

static unsigned int _netfilterRecv(unsigned int hook,
                                   struct sk_buff *skb,
                                   const struct net_device *in,
                                   const struct net_device *out,
                                   int (*okfn)(struct sk_buff*))
#else
    // not support
#endif
{
    if (_isKtunnelData(skb))
    {
        int           dataLen   = 0;
        void*         data      = NULL;
        int           handleLen = 0;
        struct iphdr* iph       = ip_hdr(skb);
        int           iphLen    = 0;
        mm_segment_t  oldfs;

        if (NULL == iph) { goto _accept; }

        iphLen  = iph->ihl << 2;
        dataLen = skb->len  - iphLen - sizeof(struct udphdr);
        data    = skb->data + iphLen + sizeof(struct udphdr);

        oldfs = get_fs();
        set_fs(KERNEL_DS);
        handleLen = _rxHandleFn(data, dataLen);
        set_fs(oldfs);

        if (0 >= handleLen) { goto _accept; }

        return NF_DROP;
    }

_accept:
    return NF_ACCEPT;
}

static void _uninitNetFilterRx(void)
{
    nf_unregister_hook(&_hookOps);
    return;
}

static int _initNetFilterRx(void)
{
    int retval;

    _hookOps.pf       = PF_INET;
    _hookOps.hooknum  = NF_INET_LOCAL_IN;
    _hookOps.priority = NF_IP_PRI_FIRST;
    _hookOps.hook     = _netfilterRecv;

    retval = nf_register_hook(&_hookOps);
    CHECK_IF(0 > retval, goto _err_return, "nf register hook failed");

    return 0;

_err_return:
    return -1;
}

static int _udpRecv(void* arg)
{
    struct msghdr   msg       = {};
    struct iovec    iov       = {};
    int             recvLen   = 0;
    int             handleLen = 0;

    CHECK_IF(NULL == _rxHandleFn, goto _recv_over, "rx handle fn is null");

    allow_signal(SIGINT);

    while (!kthread_should_stop())
    {
        // if you put the iov and msg setup out of loop
        // udp recv will get some wrong data .. ?
        // i have no idea about that  XD
        iov.iov_base = _rxBuffer;
        iov.iov_len  = BUFFER_SIZE;

        msg.msg_flags      = 0;
        msg.msg_name       = &(_rxaddr);
        msg.msg_namelen    = sizeof(struct sockaddr_in);
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;

        recvLen = sock_recvmsg(_rxsock, &msg, BUFFER_SIZE, msg.msg_flags) ;
        CHECK_IF(0 > recvLen, goto _recv_over, "sock_recvmsg failed, return value = %d", recvLen);

        handleLen = _rxHandleFn(_rxBuffer, recvLen);
        CHECK_IF(0 > handleLen, goto _recv_over, "handle rx data failed, return value = %d", handleLen);
    }

_recv_over:
    dprint("udp rx thread over");
    return 0;
}

static void _uninitUdpRx(void)
{
    if (_rxThread)
    {
        kill_pid(find_vpid(_rxThread->pid), SIGINT, 1);
        kthread_stop(_rxThread);
        _rxThread = NULL;
    }

    if (_rxsock)
    {
        sock_release(_rxsock);
        _rxsock = NULL;
    }

    return;
}

static int _initUdpRx(void)
{
    int retval = 0;

    _rxsock = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == _rxsock,      goto _err_return, "my socket failed");
    CHECK_IF(NULL == _rxsock->ops, goto _err_return, "my socket failed");

    _rxaddr.sin_family      = AF_INET;
    _rxaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    _rxaddr.sin_port        = htons(g_tunnelPort);

    retval = _rxsock->ops->bind(_rxsock, (struct sockaddr*)&_rxaddr, sizeof(struct sockaddr));
    CHECK_IF(0 > retval, goto _err_return, "bind failed, retval = %d", retval);

    _rxThread = kthread_create(_udpRecv, NULL, "UDP RX Thread");
    if (IS_ERR(_rxThread))
    {
        derror("kthread create failed");
        retval = PTR_ERR(_rxThread);
        goto _err_return;
    }

    wake_up_process(_rxThread);

    return 0;

_err_return:
    if (_rxsock)
    {
        sock_release(_rxsock);
        _rxsock = NULL;
    }
    return -1;
}

int ktunnel_initRx(char* rxmode, int (*fn)(void* data, int dataLen))
{
    int retval;

    CHECK_IF(NULL == rxmode, return -1, "rxmode is null");
    CHECK_IF(NULL == fn,     return -1, "rx handle fn is null");

    if (0 == strcmp(rxmode, "udp"))
    {
        retval = _initUdpRx();
    }
    else if (0 == strcmp(rxmode, "filter"))
    {
        retval = _initNetFilterRx();
    }
    else
    {
        derror("unknown rxmode = %s", rxmode);
        return -1;
    }

    _rxHandleFn = fn;

    _rxEnabled = true;

    return retval;
}

void ktunnel_uninitRx(char* rxmode)
{
    CHECK_IF(NULL == rxmode, return, "rxmode is null");

    if (!_rxEnabled) { return; }

    if (0 == strcmp(rxmode, "udp"))
    {
        _uninitUdpRx();
    }
    else if (0 == strcmp(rxmode, "filter"))
    {
        _uninitNetFilterRx();
    }
    else
    {
        derror("unknown rxmode = %s", rxmode);
    }

    _rxHandleFn = NULL;
    _rxEnabled  = false;

    return;
}
