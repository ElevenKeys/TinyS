#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tiny_config.h"
#include "spinlock.h"

#define DEFAULT_QUEUE_SIZE 9192 
#define MAX_QUEUE_SIZE 1024


struct message_queue {
	struct spinlock lock;
	pthread_key_t key;
	pthread_once_t once;
	int cap;
	int head;
	int tail;
	struct tiny_msg **queue;
};

static struct message_queue *Q = NULL;

void
tiny_mq_init()
{
	struct message_queue *q = malloc(sizeof(*q));
	q->cap = DEFAULT_QUEUE_SIZE;
	q->head = 0;
	q->tail = 0;
	q->queue = malloc(sizeof(struct tiny_msg*) * q->cap);
	spinlock_init(&q->lock);
	Q = q;
}



int
tiny_mq_length() 
{
	int head, tail,cap;

	spinlock_lock(&Q->lock);
	head = Q->head;
	tail = Q->tail;
	cap = Q->cap;
	spinlock_unlock(&Q->lock);
	
	if (head <= tail) {
		return tail - head;
	}
	return tail + cap - head;
}

struct tiny_msg *
tiny_mq_pop()
{
	struct tiny_msg *msg;

	spinlock_lock(&Q->lock);

	if (Q->head == Q->tail) {
	 	msg = NULL;
	} else {
		//if the message has been used, ignore it.
		do {
			msg = Q->queue[Q->head++];

			if (Q->head >= Q->cap) {
				Q->head = 0;
			}
		} while (msg->use && Q->head != Q->tail);

		if (msg->use)
			msg = NULL;
		else
			msg->use = true;
	}

	spinlock_unlock(&Q->lock);

	return msg;
}

static void
expand_queue()
{
	struct tiny_msg **new_queue = malloc(sizeof(struct tiny_msg*) * Q->cap * 2);
	int i;
	for (i=0;i<Q->cap;i++) {
		new_queue[i] = Q->queue[(Q->head + i) % Q->cap];
	}
	Q->head = 0;
	Q->tail = Q->cap;
	Q->cap *= 2;
	
	free(Q->queue);
	Q->queue = new_queue;
}

void 
tiny_mq_push(struct tiny_msg *msg)
{
	while (1) {
		spinlock_lock(&Q->lock);

		Q->queue[Q->tail] = msg;
		if (++ Q->tail >= Q->cap) {
			Q->tail = 0;
		}

		if (Q->head == Q->tail) {
			if(Q->cap < MAX_QUEUE_SIZE) {
				expand_queue(Q);
			}
			else {
				spinlock_unlock(&Q->lock);
				usleep(100);
				continue;
			}
		}

		spinlock_unlock(&Q->lock);
		break;
	}
}

