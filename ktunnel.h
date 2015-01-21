//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>

#include <linux/fs.h>             // for flip_open, filp_close
#include <linux/uaccess.h>        // for get_fs, get_ds, set_fs
#include <uapi/linux/if.h>        // for struct ifreq
#include <uapi/linux/if_tun.h>    // for TUN define value
#include <uapi/linux/if_ether.h>  // for ETH_P_IP define value
#include <uapi/linux/ip.h>        // for struct iphdr
#include <uapi/linux/udp.h>       // for struct udphdr
#include <uapi/linux/in.h>        // for IPPROTO_ICMP
#include <linux/inet.h>           // for in4_pton
#include <net/sock.h>             // for IPPROTO_IP, SOCK_DGRAM, AF_INET
#include <linux/net.h>            // for sock_create, sock_alloc_file
#include <linux/kthread.h>        // for kthread
#include <linux/netpoll.h>        // for netpoll
#include <linux/inetdevice.h>     // for struct in_device
#include <net/route.h>            // for routing table
#include <linux/netfilter_ipv4.h> // for netfilter

//////////////////////////////////////////////////////////////////////////////
//
//      Defined Values
//
//////////////////////////////////////////////////////////////////////////////
#define TAP_FILE_PATH "/dev/net/tun"
#define BUFFER_SIZE   2048

// 60 bytes is max ip header size
#define TUNNEL_HDR_SIZE (sizeof(struct ethhdr)+60+sizeof(struct udphdr))

//////////////////////////////////////////////////////////////////////////////
//
//      Macros
//
//////////////////////////////////////////////////////////////////////////////
#define dprint(a, b...) printk("%s(): "a"\n", __func__, ##b)
#define derror(a, b...) printk("[ERROR] %s(): "a"\n", __func__, ##b)

#define CHECK_IF(assertion, error_action, ...) \
{\
    if (assertion) \
    { \
        derror(__VA_ARGS__); \
        {error_action;} \
    }\
}

#define FN_APPLY_ALL(type, fn, ...) \
{\
    void* _stopPoint = (int[]){0};\
    void** _listForApplyAll = (type[]){__VA_ARGS__, _stopPoint};\
    int i;\
    for (i=0; _listForApplyAll[i] != _stopPoint; i++)\
    {\
        fn(_listForApplyAll[i]);\
    }\
}

//////////////////////////////////////////////////////////////////////////////
//
//      Type Definitions
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
//      Module Parameters
//
//////////////////////////////////////////////////////////////////////////////
extern char* g_ifname;
extern char* g_ip;
extern char* g_mask;
extern char* g_dst;  // dst real ip
extern int   g_port; // tunnel port
extern char* g_txmode;
extern char* g_rxmode;

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: My System Calls
//
//////////////////////////////////////////////////////////////////////////////
int  my_read(struct file* fp, void *buf, int count);
int  my_write(struct file* fp, const void *buf, int count);
long my_ioctl(struct file* fp, unsigned int cmd, unsigned long param);
int  my_inet_pton(int af, const char *src, void *dst);

struct socket* my_socket(int family, int type, int protocol);
struct file*   my_open(char* filename, int flags);
void           my_close(void* fp);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: TAP
//
//////////////////////////////////////////////////////////////////////////////
void ktunnel_uninitTap(void);
int  ktunnel_initTap(char* ifname, char* ipaddr, char* netmask);
int  ktunnel_writeTap(void* data, int dataLen);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: TX
//
//////////////////////////////////////////////////////////////////////////////
int  ktunnel_send(void* data, int dataLen);
void ktunnel_uninitTx(char* txmode);
int  ktunnel_initTx(char* txmode);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: RX
//
//////////////////////////////////////////////////////////////////////////////
void ktunnel_uninitRx(char* rxmode);
int  ktunnel_initRx(char* rxmode, int (*handlefn)(void* data, int dataLen) );
