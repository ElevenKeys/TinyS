#ifndef TINY_MQ_H
#define TINY_MQ_H

struct tiny_msg;
void tiny_mq_init();
int tiny_mq_length();
struct tiny_msg *tiny_mq_pop();
void tiny_mq_push(struct tiny_msg *message);

#endif
