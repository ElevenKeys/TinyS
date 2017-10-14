#ifndef TINY_CONFIG_H
#define TINY_CONFIG_H

#define STATIC 0
#define PROXY 1
#define CGI 2
#define FAST_CGI 3
#define UWSGI 4

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

#ifndef BODY_TYPE_LENGTH
#define BODY_TYPE_NONE 0
#define BODY_TYPE_LENGTH 1
#define BODY_TYPE_CHUNKED 2
#endif

#ifndef SOCK_OPEN
#define SOCK_OPEN 0
#define SOCK_R_CLOSE 1
#define SOCK_W_CLOSE 2
#define SOCK_RW_CLOSE 3

#define r_close(sockstate) sockstate |= SOCK_R_CLOSE
#define w_close(sockstate) sockstate |= SOCK_W_CLOSE
#endif

typedef struct
{
    int fd;

    char io[MAXLINE];
    char *iostart;
    char *ioend;

    char buf[MAXLINE];
    char *bufend;

    int body_type;
    size_t body_left;

    int sockstat;

    connect_ctx *peer_connect; //for reverse proxy

    state_transfer_t *handler;

    //reserved for upstream connect
	int param_pass;
    char *host;
} connect_ctx;

typedef bool (*state_transfer_t)(connect_ctx*);
typedef struct 
{
	state_transfer_t request_entry;
	state_transfer_t upstream_entry;
} module_t;

typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct spinlock spinlock;
	struct tiny_config config;
	int sleep;
} tiny_context;

int create_conn_ctx(connect_ctx **ctxp, int fd, int state);
int release_conn_ctx(connect_ctx *ctx);

extern module_t *module_set[];

#endif
