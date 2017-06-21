#ifndef TINY_CONFIG_H
#define TINY_CONFIG_H

#define PROXY 0
#define CGI 1
#define FAST_CGI 2
#define UWSGI 3

#include <pthread.h>
#include <stdbool.h>

#include "tiny_defs.h"
#include "spinlock.h"

struct proxy_server
{
	int param_pass;
	char app_addr[MAXLINE];
	char app_port[8];
};

struct tiny_config
{
	bool daemon;
	int thread;
	char agent_addr[MAXLINE];
	char agent_port[8];
	int appserver_n;
	struct proxy_server *appserver;
};

#define FORWARD 0
#define RESPONSE 1

struct tiny_msg
{
	int type;
	int param_pass;
	int fd_from;
	int fd_to;
	char *host;

	char *buf;
	size_t bufsize;

	bool use;
};

typedef void* (*pass_handler_t)(struct tiny_msg*);
typedef struct 
{
	pass_handler_t forward;
	pass_handler_t response;
} handler_module_t;

typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct spinlock spinlock;
	struct tiny_config config;
	int sleep;
} tiny_context;

extern handler_module_t *pass_handler_set[];

#endif
