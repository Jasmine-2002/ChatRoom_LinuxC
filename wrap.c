#include "wrap.h"
void perr_exit(const char *s)
{
    perror(s);
    exit(1);

}

int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr)
{
    int n;

    again:

    if ( (n = accept(fd, sa, salenptr)) < 0){
        if ((errno == ECONNABORTED) || (errno == EINTR))
            goto again;
        else
            perr_exit("accept error");
    }
    return n;

}

void Bind(int fd, const struct sockaddr *sa, socklen_t salen)
{

    if (bind(fd, sa, salen) < 0)
        perr_exit("bind error");
}

void Listen(int fd, int backlog)
{

    if (listen(fd, backlog) < 0)
        perr_exit("listen error");
}

int Socket(int family, int type, int protocol)
{
    int n;
    if ( (n = socket(family, type, protocol)) < 0)
        perr_exit("socket error");
    return n;

}


void Close(int fd)
{

    if (close(fd) == -1)
        perr_exit("close error");
}


