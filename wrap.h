#ifndef CSPROJECT_C_WRAP_H
#define CSPROJECT_C_WRAP_H

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>

void perr_exit(const char *s);
void Connect(int fd, const struct sockaddr *sa, socklen_t salen);
int Socket(int family, int type, int protocol);
ssize_t Read(int fd, void *ptr, size_t nbytes);
ssize_t Write(int fd, const void *ptr, size_t nbytes);
void Close(int fd);
char getch();

#endif //CSPROJECT_C_WRAP_H