//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      License Declarations
//
//////////////////////////////////////////////////////////////////////////////
MODULE_AUTHOR("tacolin");
MODULE_DESCRIPTION("KERNEL SPACE TUNNEL");
MODULE_LICENSE("GPL");

//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
char* g_ifname     = "tap01";
char* g_ip         = NULL;
char* g_mask       = NULL;
char* g_dstRealip  = NULL;
int   g_tunnelPort = 50000;
char* g_txmode     = "udp";
char* g_rxmode     = "udp";

module_param(g_ifname,    charp, S_IRUSR);
module_param(g_ip,        charp, S_IRUSR);
module_param(g_mask,      charp, S_IRUSR);
module_param(g_dstRealip, charp, S_IRUSR);
module_param(g_tunnelPort, int,  S_IRUSR);
module_param(g_txmode,    charp, S_IRUSR);
module_param(g_rxmode,    charp, S_IRUSR);

static mm_segment_t _oldfs;

//////////////////////////////////////////////////////////////////////////////
//
//      Static Functions : handle kernel memory access
//
//////////////////////////////////////////////////////////////////////////////
static void _setKernelFs(void)
{
    _oldfs = get_fs();
    set_fs(get_ds());
}

static void _unsetKernelFs(void)
{
    set_fs(_oldfs);
}

static int _checkModuleNecessaryParameters(void)
{
    if (NULL == g_dstRealip)
    {
        return -1;
    }

    return 0;
}

static void _showModuleParameters(void)
{
    dprint("g_ifname     = %s", g_ifname);
    dprint("g_ip         = %s", g_ip);
    dprint("g_mask       = %s", g_mask);
    dprint("g_dstRealip  = %s", g_dstRealip);
    dprint("g_tunnelPort = %d", g_tunnelPort);
    dprint("g_txmode     = %s", g_txmode);
    dprint("g_rxmode     = %s", g_rxmode);

    return;
}

//////////////////////////////////////////////////////////////////////////////
//
//      Functions : Kernel Modules Init / Exit
//
//////////////////////////////////////////////////////////////////////////////
static int __init ktunnel_init(void)
{
    int ret = -1;

    dprint("");
    dprint("===");

    _setKernelFs();

    if (0 > _checkModuleNecessaryParameters())
    {
        dprint("module parameters check failed");
        goto _ERROR;
    }

    _showModuleParameters();

    ret = ktap_init(g_ifname, g_ip, g_mask, g_txmode);
    if (0 > ret)
    {
        dprint("ktap init failed");
        goto _ERROR;
    }

    ret = kudp_init(g_dstRealip, g_tunnelPort, g_rxmode);
    if (0 > ret)
    {
        dprint("kudp init failed");
        goto _ERROR;
    }

    ret = kfilter_init(g_rxmode);
    if (0 > ret)
    {
        dprint("kfilter init failed");
        goto _ERROR;
    }

    dprint("ktuunel init ok");
    return 0;

_ERROR:
    kfilter_uninit();
    kudp_uninit();
    ktap_uninit();
    _unsetKernelFs();
    return ret;
}

static void __exit ktunnel_exit(void)
{
    kfilter_uninit();
    kudp_uninit();
    ktap_uninit();
    _unsetKernelFs();

    dprint("ktuunel exit ok");
    dprint("===");
    dprint("");

    return;
}


module_init( ktunnel_init );
module_exit( ktunnel_exit );
