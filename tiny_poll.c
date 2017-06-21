#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_EVENT_ONCE 1000

static int epoll_fd;
static struct epoll_event evs[MAX_EVENT_ONCE];

int
poll_create()
{
	epoll_fd = epoll_create(1024);
	return epoll_fd;
}

int
poll_release()
{
	return close(epoll_fd);
}

int
set_noblock(int fd)
{
	int flag = fcntl(fd, F_GETFL, 0);
	if (flag == -1)
		return -1;

	if(fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1)
		return -1;
	return 0;
}

int
poll_add(int fd, void *ud)
{
	struct epoll_event ev;
	/*set_noblock(fd);*/
	ev.data.ptr = ud;
	ev.events = EPOLLIN|EPOLLET;
	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

int
poll_del(int fd)
{
	return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int
poll_wait(void **uds, int max)
{
	int n, i;

	if (max > MAX_EVENT_ONCE)
		max = MAX_EVENT_ONCE;

	n = epoll_wait(epoll_fd, evs, max, -1);
	for(i = 0; i < n; i++){
		uds[i] = evs[i].data.ptr;
	}
	return n;
}
