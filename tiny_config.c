#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "tiny_log.h"
#include "tiny_config.h"

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
struct spinlock g_spinlock = SPINLOCK_INITIALIZER;
struct tiny_config g_config;
int g_sleep;

int
create_conn_ctx(connect_ctx **ctxp, int fd, int state)
{
	connect_ctx *ctx = (connect_ctx*)calloc(sizeof(connect_ctx));
	if (!ctx)
		tiny_error("malloc error");

	ctx->fd = fd;
	ctx->state = state;
	*ctxp = ctx;

	return 0;
}

int
release_conn_ctx(connect_ctx *ctx)
{
	free(ctx);
	return 0;
}
