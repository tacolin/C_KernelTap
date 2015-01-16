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
#define TAP_FILE_PATH   "/dev/net/tun"

// 60 bytes is max ip header size
#define TUNNEL_HDR_SIZE (sizeof(struct ethhdr)+60+sizeof(struct udphdr))

#define BUFFER_SIZE 2048
#define IPADDR_SIZE 20

//////////////////////////////////////////////////////////////////////////////
//
//      Macros
//
//////////////////////////////////////////////////////////////////////////////
#define dprint(a, b...) printk("%s(): "a"\n", __func__, ##b)

//////////////////////////////////////////////////////////////////////////////
//
//      Type Definitions
//
//////////////////////////////////////////////////////////////////////////////
typedef int (*my_threadFn)(void * data);

//////////////////////////////////////////////////////////////////////////////
//
//      Module Parameters
//
//////////////////////////////////////////////////////////////////////////////
extern char* g_ifname;
extern char* g_ip;
extern char* g_mask;
extern char* g_dstRealip;
extern int   g_tunnelPort;
extern char* g_txmode;
extern char* g_rxmode;

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: My System Calls (ksyscall)
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
//      Function Declarations: Kernel TAP (ktap)
//
//////////////////////////////////////////////////////////////////////////////
int  ktap_init(char* ifname, char* ipaddr, char* netmask, char* txmode);
void ktap_uninit(void);
int  ktap_write(void* data, int dataLen);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: Kernel UDP (ktap)
//
//////////////////////////////////////////////////////////////////////////////
int  kudp_init(char* dstip, int tunnelport, char* rxmode);
void kudp_uninit(void);
int  kudp_send(void* data, int dataLen);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: Kernel Netpoll (knetpoll)
//
//////////////////////////////////////////////////////////////////////////////
bool knetpoll_getInfo(char* dstip, struct netpoll* np);
int  knetpoll_send(struct netpoll* np, void* data, int dataLen);

//////////////////////////////////////////////////////////////////////////////
//
//      Function Declarations: Netfilter Hook (kfilter)
//
//////////////////////////////////////////////////////////////////////////////
int  kfilter_init(char* rxmode);
void kfilter_uninit(void);
