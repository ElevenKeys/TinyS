#ifndef TINY_SOCKET_H
#define TINY_SOCKET_H

#include <unistd.h>
#include <arpa/inet.h>

typedef struct sockaddr SA;
int tcp_connect(const char *host, const char *serv);
int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp);
char *sock_ntop(struct sockaddr *sa, socklen_t len);

#endif
