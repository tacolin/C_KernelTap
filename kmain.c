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

    ret = ktap_init(TAP_IF_NAME, TAP_IF_IP, TAP_IF_NETMASK);
    if (0 > ret)
    {
        dprint("ktap init failed");
        goto _ERROR;
    }

    ret = kudp_init(DST_REAL_IP, TUNNEL_PORT);
    if (0 > ret)
    {
        dprint("kudp init failed");
        goto _ERROR;
    }

    dprint("ktuunel init ok");
    return 0;

_ERROR:
    kudp_uninit();
    ktap_uninit();
    _unsetKernelFs();
    return ret;
}

static void __exit ktunnel_exit(void)
{
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
