#ifndef TINY_POLL_H
#define TINY_POLL_H

int poll_create();
int poll_release();
int poll_add(int fd, void *ud);
int poll_del(int fd);
int poll_wait(void **uds, int max);
int set_noblock(int fd);
#endif
