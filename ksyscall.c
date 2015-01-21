//////////////////////////////////////////////////////////////////////////////
//
//      Headers
//
//////////////////////////////////////////////////////////////////////////////
#include "ktunnel.h"

//////////////////////////////////////////////////////////////////////////////
//
//      Functions
//
//////////////////////////////////////////////////////////////////////////////
int my_read(struct file* fp, void *buf, int count)
{
    CHECK_IF(NULL == fp,       return -1, "fp is null");
    CHECK_IF(NULL == fp->f_op, return -1, "fp ops is null");

    return fp->f_op->read(fp, buf, count, &(fp->f_pos));
}

int my_write(struct file* fp, const void *buf, int count)
{
    CHECK_IF(NULL == fp,       return -1, "fp is null");
    CHECK_IF(NULL == fp->f_op, return -1, "fp ops is null");

    return fp->f_op->write(fp, buf, count, &(fp->f_pos));
}

long my_ioctl(struct file* fp, unsigned int cmd, unsigned long param)
{
    CHECK_IF(NULL == fp,       return -1, "fp is null");
    CHECK_IF(NULL == fp->f_op, return -1, "fp ops is null");

    return fp->f_op->unlocked_ioctl(fp, cmd, param);
}

struct socket* my_socket(int family, int type, int protocol)
{
    int            retval;
    struct socket* socket = NULL;
    struct file*   fp     = NULL;

    retval = sock_create(family, type, protocol, &socket);
    CHECK_IF(0 > retval,     goto err_return, "sock create failed");
    CHECK_IF(NULL == socket, goto err_return, "sock is null");

    fp = sock_alloc_file(socket, 0, NULL);
    CHECK_IF(NULL == fp, goto err_return, "socket alloc file failed");

    return socket;

err_return:
    if (socket) { sock_release(socket); }
    return NULL;
}

int my_inet_pton(int af, const char *src, void *dst)
{
    if (AF_INET == af)
    {
        return in4_pton(src, strlen(src), (u8*)dst, '\0', NULL);
    }
    else if (AF_INET6 == af)
    {
        return in6_pton(src, strlen(src), (u8*)dst, '\0', NULL);
    }
    else
    {
        dprint("uknown af value = %d", af);
        return -1;
    }
}

struct file* my_open(char* filename, int flags)
{
    CHECK_IF(NULL == filename, return NULL, "file name is null");

    return filp_open(filename, flags, 0);
}

void my_close(void* fp)
{
    if (NULL != fp)
    {
        filp_close(fp, NULL);
    }
}
