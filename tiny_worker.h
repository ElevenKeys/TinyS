#ifndef TINY_WORKER_H
#define TINY_WORKER_H

void *routine(void *arg);
void closesock(struct tiny_msg *msg);
void read_ioerror(struct tiny_msg *msg);

#endif
