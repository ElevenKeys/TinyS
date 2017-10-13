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

#ifndef LINE
#define LINE 0
#define HEADER 1
#define BODY 2
#endif

#ifndef CHUNKED
#define LENGTH 0
#define CHUNKED 1
#endif

typedef struct
{
    int phrase;
    int fd;
    int body_encoding;
    char buf[BUFFSIZE];
    size_t bufsize;

    char *host;
} connect_ctx;

struct tiny_msg
{
	int param_pass;
    connect_ctx request;
    connect_ctx upstream;
};

typedef void* (*pass_handler_t)(struct tiny_msg*);
typedef struct 
{
	pass_handler_t request_handler;
	pass_handler_t upstream_handler;
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
