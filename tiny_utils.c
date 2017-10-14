#include <stdlib.h>
#include <assert.h>

#include "tiny_config.h"
#include "tiny_io.h"
#include "tiny_log.h"
#include "tiny_defs.h"
#include "tiny_utils.h"

void
closeread(connect_ctx *ctx)
{
	int fd = ctx->fd;
	assert(fd >= 0);

	if (!ctx->peer_connect) {
		closewrite(ctx->peer_connect);
	}

	r_close(ctx->sockstate);
	if (poll_del(fd) < 0)
		tiny_error("%s", "poll_del error");

	if (ctx->sockstate == SOCK_RW_CLOSE)
	{
		if (close(fd) < 0)
			tiny_error("%s", "close socket error");

		release_conn_ctx(ctx);
	}
}

void
closewrite(connect_ctx *ctx)
{
	int fd = ctx->fd;
	assert(fd >= 0);

	w_close(ctx->sockstate);
	if (ctx->sockstate == SOCK_RW_CLOSE)
	{
		if (close(fd) < 0)
			tiny_error("%s", "close socket error");

		release_conn_ctx(ctx);
	}
}

void
close(connect_ctx *ctx)
{
    closeread(ctx);
    closewrite(ctx);
}

static void
read_ioerror(connect_ctx *ctx)
{
	switch (errno) {
		case ECONNRESET:
			tiny_notice("unexpected reset");
            close(ctx);
			break;
		default:
			tiny_error("read socket error");
	}
}

int
readline_wrapper(connect_ctx *ctx, char *buf, size_t size)
{
	ssize_t readcnt;

	readcnt = tiny_readline(ctx->fd, buf, size, CRLF);
	if (readcnt < 0) {
		read_ioerror(ctx);
		return -1;
	} else if (readcnt == 0) {
		return 0;
	}

	return readcnt;
}

int
readn_wrapper(connect_ctx *ctx, char *buf, size_t toberead)
{
	ssize_t readcnt;

	readcnt = tiny_readn(ctx->fd, buf, toberead);
	if (readcnt < 0) {
		read_ioerror(ctx);
		return -1;
	} else if (readcnt == 0) {
		return 0;
	}

	return readcnt;
}

ssize_t
read_wrapper(connect_ctx *ctx)
{
	ssize_t len;
	
	while (ctx->iostart == ctx->ioend) {
		len = read(ctx->fd, ctx->io, MAXLINE);
		if (len < 0) {
			switch (errno) {
				case EINTR:
					continue;
                    break;
				case EAGAIN:
#if EAGAIN != EWOULDBLOCK
				case EWOULDBLOCK:
#endif
                    return IO_BLOCK;
				default:
                    read_ioerror(ctx);
					return IO_ERROR;
			}
		}
		else if (len == 0)
            closeread(ctx);
            tiny_notice("unexpected close")
			return IO_CLOSE;
		else {
            ctx->iostart = ctx->io;
            ctx->ioend = ctx->iostart + len;
            return len;
		}
	}
    return ctx->ioend - ctx->iostart;
}

ssize_t
write_wrapper(connect_ctx *ctx, char *buf, size_t n)
{
	size_t n_left = n;
	ssize_t n_writen;

	while (n_left > 0) {
		n_writen = write(ctx->fd, buf, n_left);
		if (n_writen < 0) {
			if(errno == EINTR)
				continue;
			else
				return -1;
		}
		n_left -= n_writen;
		buf += n_writen;
	}
	return n;
}
void
writen_wrapper(connect_ctx *ctx, char *buf, size_t size)
{
	if (tiny_writen(ctx->fd, buf, size) < 0)
		tiny_error("write socket error");
}

void
dispatch(struct tiny_msg *msg)
{
	//distribute the request according to the balacing strategy.
	static int index=0;
	struct proxy_server *app;

	msg->type = RESPONSE;

	//connect the client with appplication server
	if(msg->fd_to < 0) {
		spinlock_lock(&g_ctx->spinlock);
		index %= g_ctx->config.appserver_n; 
		app = g_ctx->config.appserver + index++;
		spinlock_unlock(&g_ctx->spinlock);

		msg->param_pass = app->param_pass;
		msg->host = app->app_addr;
		msg->fd_to = tcp_connect(app->app_addr, app->app_port);

		poll_add(msg->fd_to, msg);
	}
}

void
clienterror(int fd, int errnum, char *errmsg)
{
	char buf[BUFFSIZE];

	sprintf(buf, "HTTP/1.1 %d %s\r\n", errnum, errmsg);
	sprintf(buf, "%s%s\r\n\r\n", buf, errmsg);
	tiny_writen(fd, buf, strlen(buf));
}
