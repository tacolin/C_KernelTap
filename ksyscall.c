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
    if (NULL == fp)
    {
        dprint("fp is null");
        return -1;
    }

    if (NULL == fp->f_op)
    {
        dprint("fp ops is null");
        return -1;
    }

    return fp->f_op->read(fp, buf, count, &(fp->f_pos));
}

int my_write(struct file* fp, const void *buf, int count)
{
    if (NULL == fp)
    {
        dprint("fp is null");
        return -1;
    }

    if (NULL == fp->f_op)
    {
        dprint("fp ops is null");
        return -1;
    }

    return fp->f_op->write(fp, buf, count, &(fp->f_pos));
}

long my_ioctl(struct file* fp, unsigned int cmd, unsigned long param)
{
    if (NULL == fp)
    {
        dprint("fp is null");
        return -1;
    }

    if (NULL == fp->f_op)
    {
        dprint("fp ops is null");
        return -1;
    }

    return fp->f_op->unlocked_ioctl(fp, cmd, param);
}

struct socket* my_socket(int family, int type, int protocol)
{
    struct socket* socket = NULL;
    struct file*   fp     = NULL;

    if (0 > sock_create(family, type, protocol, &socket))
    {
        dprint("sock create failed");
        return NULL;
    }

    if (NULL == socket)
    {
        dprint("created socket is null");
        return NULL;
    }

    fp = sock_alloc_file(socket, 0, NULL);
    if (NULL == fp)
    {
        dprint("socket alloc file failed");
        sock_release(socket);
        return NULL;
    }

    return socket;
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
    if (NULL == filename)
    {
        dprint("filename is null");
        return NULL;
    }

    return filp_open(filename, flags, 0);
}

void my_close(void* fp)
{
    if (NULL != fp)
    {
        filp_close(fp, NULL);
    }
}
