//////////////////////////////////////////////////////////////////////////////
//      Headers
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//      Global Variables
//////////////////////////////////////////////////////////////////////////////
static bool                _tapEnabled = false;
static struct file*        _tapFile    = NULL;
static struct task_struct* _readThread = NULL;

static unsigned char _readBuffer[BUFFER_SIZE] = {0};

//////////////////////////////////////////////////////////////////////////////
//      Static Functions
//////////////////////////////////////////////////////////////////////////////
static struct file* _alloc(char* filename, char* ifname, int flags)
{
    struct ifreq ifr    = {};
    struct file* tapfp  = NULL;
    long         retval = 0;

    CHECK_IF(NULL == filename, goto err_return, "filename is null");
    CHECK_IF(NULL == ifname,   goto err_return, "ifname is null");

    tapfp = my_open(filename, O_RDWR);
    CHECK_IF(NULL == tapfp, goto err_return, "my open failed");

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags = flags;

    retval = my_ioctl(tapfp, TUNSETIFF, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl failed");

    return tapfp;

err_return:
    if (tapfp) { my_close(tapfp); }
    return NULL;
}

static int _setIpaddr(char* ifname, char* ipaddr)
{
    struct ifreq   ifr    = {};
    long           retval = 0;
    struct socket* socket = NULL;

    CHECK_IF(NULL == ifname, goto err_return, "ifname is null");
    CHECK_IF(NULL == ipaddr, goto err_return, "ipaddr is null");

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    retval = my_inet_pton(AF_INET, ipaddr, ifr.ifr_addr.sa_data+2);
    CHECK_IF(0 > retval, goto err_return, "my inet pton failed");

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == socket,       goto err_return, "my socket failed");
    CHECK_IF(NULL == socket->file, goto err_return, "socket->file is null");

    retval = my_ioctl(socket->file, SIOCSIFADDR, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl set failed");

    sock_release(socket);
    return 0;

err_return:
    if (socket) { sock_release(socket); }
    return -1;
}

static int _setMtuSize(char* ifname)
{
    struct ifreq   ifr    = {};
    long           retval = 0;
    struct socket* socket = NULL;

    CHECK_IF(NULL == ifname, goto err_return, "ifname is null");

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == socket, goto err_return, "my socket failed");

    retval = my_ioctl(socket->file, SIOCGIFMTU, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl get mtu size failed");

    CHECK_IF(TUNNEL_HDR_SIZE >= ifr.ifr_mtu, goto err_return, "mtu size = %d is too small to use tunnel", ifr.ifr_mtu);

    ifr.ifr_mtu -= TUNNEL_HDR_SIZE;

    retval = my_ioctl(socket->file, SIOCSIFMTU, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl set mtu size failed");

    sock_release(socket);

    return 0;

err_return:
    if (socket) { sock_release(socket); }
    return -1;
}

static int _setNetmask(char* ifname, char* netmask)
{
    struct ifreq   ifr    = {};
    long           retval = 0;
    struct socket* socket = NULL;

    CHECK_IF(NULL == ifname,  goto err_return, "ifname is null");
    CHECK_IF(NULL == netmask, goto err_return, "netmask is null");

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    retval = my_inet_pton(AF_INET, netmask, ifr.ifr_addr.sa_data+2);
    CHECK_IF(0 > retval, goto err_return, "my inet pton failed");

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == socket,       goto err_return, "my socket failed");
    CHECK_IF(NULL == socket->file, goto err_return, "socket->file is null");

    retval = my_ioctl(socket->file, SIOCSIFNETMASK, (unsigned long)&ifr);
    CHECK_IF(0> retval, goto err_return, "my ioctl failed");

    sock_release(socket);
    return 0;

err_return:
    if (socket) { sock_release(socket); }
    return -1;
}

static int _enableInterface(char* ifname)
{
    struct ifreq   ifr    = {};
    long           retval = 0;
    struct socket* socket = NULL;

    CHECK_IF(NULL == ifname, goto err_return, "ifname is null");

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;

    socket = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    CHECK_IF(NULL == socket,       goto err_return, "my socket failed");
    CHECK_IF(NULL == socket->file, goto err_return, "socket->file is null");

    retval = my_ioctl(socket->file, SIOCGIFFLAGS, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl get flags failed");

    ifr.ifr_flags |= ( IFF_UP | IFF_RUNNING );

    retval = my_ioctl(socket->file, SIOCSIFFLAGS, (unsigned long)&ifr);
    CHECK_IF(0 > retval, goto err_return, "my ioctl set flags failed");

    sock_release(socket);

    return 0;

err_return:
    if (socket) { sock_release(socket); }
    return -1;
}

static bool _isTapWantedData(void* data, int dataLen)
{
    struct ethhdr* ethhdr  = NULL;
    struct iphdr*  iphdr   = NULL;
    u16            ethtype = 0;

    CHECK_IF(NULL == data, return false, "data is null");
    CHECK_IF(ETH_HLEN >= dataLen, return false, "dataLen = %d is too short", dataLen);

    ethhdr  = (struct ethhdr*)data;
    ethtype = ntohs(ethhdr->h_proto);
    if (ethtype == ETH_P_IP) // IP protocol
    {
        data += sizeof(struct ethhdr);
        iphdr = (struct iphdr*)data;
        if (iphdr->protocol == IPPROTO_ICMP) // ICMP
        {
            return true;
        }
        else if (iphdr->protocol == IPPROTO_TCP)
        {
            return true;
        }
        else if (iphdr->protocol == IPPROTO_UDP)
        {
            return true;
        }
    }
    else if (ethtype == ETH_P_ARP) // ARP
    {
        return true;
    }

    return false;
}

static int _tapRead(void *arg)
{
    int  readLen = 0;
    int  sendLen = 0;

    allow_signal(SIGINT);

    while (!kthread_should_stop())
    {
        readLen = my_read(_tapFile, _readBuffer, BUFFER_SIZE);
        CHECK_IF(0 >= readLen, goto read_over, "readLen = %d failed", readLen);

        if (_isTapWantedData(_readBuffer, readLen))
        {
            sendLen = ktunnel_send(_readBuffer, readLen);
            // CHECK_IF(0 >= sendLen, goto read_over, "sendLen = %d failed", sendLen);
        }
    }

read_over:
    dprint("over");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//      Functions
//////////////////////////////////////////////////////////////////////////////
int ktunnel_writeTap(void* data, int dataLen)
{
    CHECK_IF(NULL == _tapFile, return -1, "tap file is null");
    CHECK_IF(NULL == data,     return -1, "data is null");
    CHECK_IF(0 == dataLen,     return -1, "dataLen = 0");

    return my_write(_tapFile, data, dataLen);
}

int ktunnel_initTap(char* ifname, char* ipaddr, char* netmask)
{
    int retval = 0;

    CHECK_IF(NULL == ifname, goto err_return, "ifname is null");

    _tapFile = _alloc(TAP_FILE_PATH, ifname, IFF_TAP | IFF_NO_PI);
    CHECK_IF(NULL == _tapFile, goto err_return, "alloc tap fp failed");

    if (ipaddr)
    {
        retval = _setIpaddr(ifname, ipaddr);
        CHECK_IF(0 > retval, goto err_return, "tap set ip failed");
    }

    if (netmask)
    {
        retval = _setNetmask(ifname, netmask);
        CHECK_IF(0 > retval, goto err_return, "tap set netmask failed");
    }

    retval = _setMtuSize(ifname);
    CHECK_IF(0 > retval, goto err_return, "tap set mtu size failed");

    retval = _enableInterface(ifname);
    CHECK_IF(0 > retval, goto err_return, "tap enable interface failed");

    _readThread = kthread_create(_tapRead, NULL, "TAP READ Thread");
    if (IS_ERR(_readThread))
    {
        derror("kthread create failed");
        retval = PTR_ERR(_readThread);
        goto err_return;
    }

    wake_up_process(_readThread);

    _tapEnabled = true;

    return 0;

err_return:
    if (_tapFile)
    {
        my_close(_tapFile);
        _tapFile = NULL;
    }
    return -1;
}

void ktunnel_uninitTap(void)
{
    if (!_tapEnabled) { return; }

    if (_readThread)
    {
        kill_pid(find_vpid(_readThread->pid), SIGINT, 1);
        kthread_stop(_readThread);
    }

    if (_tapFile)
    {
        my_close(_tapFile);
    }

    _tapFile    = NULL;
    _readThread = NULL;
    _tapEnabled = false;

    return;
}
