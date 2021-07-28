#include "wrap.h"
void perr_exit(const char *s)
{
    perror(s);
    exit(1);

}


void Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{

    if (connect(fd, sa, salen) < 0)
        perr_exit("connect error");
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


char getch(void)
{
    struct termios tm,tm_old;
    int fd=STDIN_FILENO,c;
    if(tcgetattr(fd,&tm)<0)
        return -1;
    tm_old=tm;
    cfmakeraw(&tm);
    if(tcsetattr(fd,TCSANOW,&tm)<0)
        return -1;
    c=fgetc(stdin);
    if(tcsetattr(fd,TCSANOW,&tm_old)<0)
        return -1;
    if(c==3)
        exit(1);
    return c;
}
