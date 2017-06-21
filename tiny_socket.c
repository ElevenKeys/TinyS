#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "tiny_log.h"
#include "tiny_defs.h"

int
tcp_connect(const char *host, const char *serv)
{
	int sockfd, n;
	struct addrinfo hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
		tiny_error("get addrinfo error for %s %s:%s",
				host, serv, gai_strerror(n));

	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue;

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;

		close(sockfd);
	} while ((res = res->ai_next) != NULL);

	if (res == NULL)
		tiny_error("connect error for %s %s", host, serv);

	freeaddrinfo(ressave);

	return sockfd;
}

int
tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
	int listenfd, n;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
		tiny_error("get addrinfo error for %s %s:%s",
				host, serv, gai_strerror(n));
	ressave = res;

	do {
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)
			continue;

		if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
			tiny_error("set socket option error");

		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break;

		close(listenfd);
	} while ((res = res->ai_next) != NULL);

	if (res == NULL)
		tiny_error("tcp_listen for %s %s error", host, serv);

	if (listen(listenfd, LISTENQ) < 0)
		tiny_error("listen error");

	if(addrlenp)
		*addrlenp = res->ai_addrlen;

	freeaddrinfo(ressave);

	return listenfd;
}

char *
sock_ntop(const struct sockaddr *sa, socklen_t len)
{
	static char str[MAXLINE];
	struct sockaddr_in *sin = (struct sockaddr_in*)sa;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)sa;

	switch (sa->sa_family) {
		case AF_INET:
			if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
				tiny_error("inet_ntop error");

			sprintf(str, "%s:%d", str, ntohs(sin->sin_port));
			break;
		case AF_INET6:
			if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
				tiny_error("inet_ntop error");

			snprintf(str, sizeof(str), "%s:%d", str, ntohs(sin6->sin6_port));
			break;
	}
	return str;
}
