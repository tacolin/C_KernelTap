//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      Type Definitions
//
//////////////////////////////////////////////////////////////////////////////
struct my_filter
{
    bool enable;
    struct nf_hook_ops hkop;
};

//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
static struct my_filter _kfilter = {};


//////////////////////////////////////////////////////////////////////////////
//
//      Static Functions
//
//////////////////////////////////////////////////////////////////////////////
static bool _isWantedData(struct sk_buff *skb)
{
    struct iphdr* iph = NULL;
    struct udphdr* udph = NULL;
    u16 srcport = 0;
    u16 dstport = 0;

    if (NULL == skb)
    {
        return false;
    }

    iph = ip_hdr(skb);
    if (NULL == iph)
    {
        return false;
    }

    if (iph->protocol != IPPROTO_UDP)
    {
        return false;
    }

    udph = udp_hdr(skb);
    if (NULL == udph)
    {
        return false;
    }

    srcport = ntohs(udph->source);
    dstport = ntohs(udph->dest);
    if ((g_tunnelPort != srcport) || (g_tunnelPort != dstport))
    {
        return false;
    }

    return true;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)

static unsigned int _processHookLocalIn(const struct nf_hook_ops *ops,
                                         struct sk_buff *skb,
                                         const struct net_device *in,
                                         const struct net_device *out,
                                         int (*okfn)(struct sk_buff*))

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)

static unsigned int _processHookLocalIn(unsigned int hook,
                                         struct sk_buff *skb,
                                         const struct net_device *in,
                                         const struct net_device *out,
                                         int (*okfn)(struct sk_buff*))
#else
    // not support
#endif
{
    if (_isWantedData(skb))
    {
        int           dataLen  = 0;
        void*         data     = NULL;
        int           writeLen = 0;
        struct iphdr* iph      = NULL;
        int           iphLen   = 0;
        mm_segment_t oldfs;

        iph = ip_hdr(skb);
        if (NULL == iph)
        {
            goto _END;
        }

        iphLen = iph->ihl << 2;

        dataLen = skb->len  - iphLen - sizeof(struct udphdr);
        data    = skb->data + iphLen + sizeof(struct udphdr);

        // the netfilter hook function belongs another kerenel module (thread)
        // changing the kernel fs is necessary.
        oldfs = get_fs();
        set_fs(get_ds());
        writeLen = ktap_write(data, dataLen);
        set_fs(oldfs);
        if (0 >= writeLen)
        {
            goto _END;
        }
        return NF_DROP;
    }

_END:
    return NF_ACCEPT;
}

//////////////////////////////////////////////////////////////////////////////
//
//      Functions
//
//////////////////////////////////////////////////////////////////////////////
int kfilter_init(char* rxmode)
{
    int ret;

    if (NULL == rxmode)
    {
        dprint("rxmode is null");
        goto _ERROR;
    }

    if (0 != strcmp(rxmode, "filter"))
    {
        return 0;
    }

    _kfilter.hkop.pf = PF_INET;
    _kfilter.hkop.hooknum = NF_INET_LOCAL_IN;
    _kfilter.hkop.priority = NF_IP_PRI_FIRST;
    _kfilter.hkop.hook = _processHookLocalIn;

    ret = nf_register_hook(&(_kfilter.hkop));
    if (0 > ret)
    {
        dprint("nf register hook failed");
        goto _ERROR;
    }

    _kfilter.enable = true;

    dprint("ok");

    return 0;

_ERROR:
    return -1;
}

void kfilter_uninit(void)
{
    if (_kfilter.enable)
    {
        nf_unregister_hook(&(_kfilter.hkop));
        _kfilter.enable = false;
        dprint("ok");
    }

    return;
}
