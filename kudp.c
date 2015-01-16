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
struct my_kudp
{
    bool enable;
    char dstip[IPADDR_SIZE];
    int  tunnelport;

    unsigned int convertedDstip;

    struct socket* sock;

    struct sockaddr_in txaddr;
    struct sockaddr_in rxaddr;

    struct task_struct* processThread;
    int interruptNum;

    unsigned char buffer[BUFFER_SIZE];
};

//////////////////////////////////////////////////////////////////////////////
//
//      Global Variables
//
//////////////////////////////////////////////////////////////////////////////
static struct my_kudp _kudp = {};

//////////////////////////////////////////////////////////////////////////////
//
//      Static Functions
//
//////////////////////////////////////////////////////////////////////////////
static int _processUdpRecvData(void* arg)
{
    struct my_kudp* kudp = (struct my_kudp*)arg;
    struct msghdr   msg;
    struct iovec    iov;
    int  readLen  = 0;
    int  writeLen = 0;

    if (NULL == arg)
    {
        dprint("arg is null");
        return 0;
    }

    allow_signal(kudp->interruptNum);

    while (!kthread_should_stop())
    {
        // if you put the iov and msg setup out of loop
        // udp recv will get some wrong data .. ?
        // i have no idea about that  XD
        iov.iov_base = kudp->buffer;
        iov.iov_len  = BUFFER_SIZE;

        msg.msg_flags      = 0;
        msg.msg_name       = &(kudp->rxaddr);
        msg.msg_namelen    = sizeof(struct sockaddr_in);
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;

        readLen = sock_recvmsg(kudp->sock, &msg,
                               BUFFER_SIZE, msg.msg_flags);
        if (0 >= readLen)
        {
            dprint("readLen = %d <= 0", readLen);
            break;
        }

        writeLen = ktap_write(kudp->buffer, readLen);
        if (0 >= writeLen)
        {
            dprint("ktap write failed, writeLen = %d", writeLen);
            break;
        }

        // dprint("writeLen = %d", writeLen);
    }

    dprint("over");
    return 0;
}

static int _setupUdpTx(struct my_kudp* kudp)
{
    if (NULL == kudp)
    {
        dprint("kudp is null");
        goto _ERROR;
    }

    kudp->txaddr.sin_family = AF_INET;
    kudp->txaddr.sin_addr.s_addr = kudp->convertedDstip;
    kudp->txaddr.sin_port = htons(kudp->tunnelport);

    dprint("ok");
    return 0;

_ERROR:
    if (NULL ==  kudp->sock)
    {
        sock_release(kudp->sock);
        kudp->sock = NULL;
    }
    return -1;
}

static int _setupUdpRx(struct my_kudp *kudp)
{
    int ret;

    if (NULL == kudp)
    {
        dprint("kudp is null");
        goto _ERROR;
    }

    kudp->rxaddr.sin_family = AF_INET;
    kudp->rxaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    kudp->rxaddr.sin_port = htons(kudp->tunnelport);

    ret = kudp->sock->ops->bind(kudp->sock,
                                  (struct sockaddr*)&(kudp->rxaddr),
                                  sizeof(struct sockaddr));
    if (0 > ret)
    {
        dprint("bind rx sock failed");
        goto _ERROR;
    }

    dprint("ok");
    return 0;

_ERROR:
    if (NULL == kudp->sock)
    {
        sock_release(kudp->sock);
        kudp->sock = NULL;
    }
    return -1;
}

static int _initUdpProcessThread(struct my_kudp *kudp,
                                 my_threadFn fn,
                                 int irqNum)
{
    if (NULL == kudp)
    {
        dprint("kudp is null");
        return -1;
    }

    if (NULL == fn)
    {
        dprint("thread fn is null");
        return -1;
    }

    kudp->processThread = kthread_create(fn, kudp, "udp process thread");
    if (IS_ERR(kudp->processThread))
    {
        dprint("kthread create failed");
        PTR_ERR(kudp->processThread);
        return -1;
    }

    memset(kudp->buffer, 0, BUFFER_SIZE);
    kudp->interruptNum = irqNum;

    return 0;
}

static int _uninitUdpProcessThread(struct my_kudp *kudp)
{
    return 0;
}

static int _startUdpProcessThread(struct my_kudp *kudp)
{
    if (NULL == kudp)
    {
        dprint("kudp is null");
        return -1;
    }

    if (NULL == kudp->processThread)
    {
        dprint("process thread is not initialized");
        return -1;
    }

    wake_up_process(kudp->processThread);
    return 0;
}

static int _stopUdpProcessThread(struct my_kudp *kudp)
{
    if (NULL == kudp)
    {
        dprint("kudp is null");
        return -1;
    }

    if (NULL == kudp->processThread)
    {
        dprint("process thread is not initialized");
        return 0;
        // return -1;
    }

    kill_pid(find_vpid(kudp->processThread->pid), kudp->interruptNum, 1);
    kthread_stop(kudp->processThread);
    kudp->processThread = NULL;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
//      Functions
//
//////////////////////////////////////////////////////////////////////////////
int kudp_send(void* data, int dataLen)
{
    struct msghdr msg;
    struct iovec  iov;

    if (NULL == data)
    {
        dprint("data is null");
        return -1;
    }

    if (0 == dataLen)
    {
        dprint("dataLen = 0");
        return -1;
    }

    iov.iov_base = data;
    iov.iov_len  = dataLen;

    msg.msg_flags      = 0;
    msg.msg_name       = &(_kudp.txaddr);
    msg.msg_namelen    = sizeof(struct sockaddr_in);
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;

    return sock_sendmsg(_kudp.sock, &msg, dataLen);
}

int kudp_init(char* dstip, int tunnelport, char* rxmode)
{
    int ret = 0;

    if (NULL == dstip)
    {
        dprint("dst ip is null");
        goto _ERROR;
    }

    if (NULL == rxmode)
    {
        dprint("rxmode is null");
        goto _ERROR;
    }

    _kudp.tunnelport = tunnelport;

    ret = my_inet_pton(AF_INET, dstip, &(_kudp.convertedDstip));
    if (0 > ret)
    {
        dprint("my inet pton failed");
        goto _ERROR;
    }

    _kudp.sock = my_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (NULL == _kudp.sock)
    {
        dprint("my socket failed");
        goto _ERROR;
    }

    if (NULL == _kudp.sock->ops)
    {
        dprint("socket ops is null");
        goto _ERROR;
    }

    ret = _setupUdpTx(&_kudp);
    if (0 > ret)
    {
        dprint("setup udp tx failed");
        goto _ERROR;
    }

    ret = _setupUdpRx(&_kudp);
    if (0 > ret)
    {
        dprint("setup udp rx failed");
        goto _ERROR;
    }

    if (0 == strcmp(rxmode, "udp"))
    {
        ret = _initUdpProcessThread(&_kudp, _processUdpRecvData, SIGINT);
        if (0 > ret)
        {
            dprint("init udp process thread failed");
            goto _ERROR;
        }

        ret = _startUdpProcessThread(&_kudp);
        if (0 > ret)
        {
            dprint("start udp process thread failed");
            goto _ERROR;
        }
    }

    _kudp.enable = true;

    dprint("dstip = %s, tunnelport = %d", dstip, tunnelport);
    dprint("ok");
    return 0;

_ERROR:
    _kudp.enable = false;
    if (_kudp.sock)
    {
        sock_release(_kudp.sock);
    }
    return -1;
}

void kudp_uninit(void)
{
    if (false == _kudp.enable)
    {
        dprint("kudp is not initialized");
        return;
    }

    _stopUdpProcessThread(&_kudp);
    _uninitUdpProcessThread(&_kudp);

    sock_release(_kudp.sock);

    dprint("ok");
    return;
}
