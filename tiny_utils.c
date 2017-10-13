
static void
clienterror(int fd, int errnum, char *errmsg)
{
	char buf[BUFFSIZE];

	sprintf(buf, "HTTP/1.1 %d %s\r\n", errnum, errmsg);
	sprintf(buf, "%s%s\r\n\r\n", buf, errmsg);
	tiny_writen(fd, buf, strlen(buf));
}

void
closesock(struct tiny_msg *msg)
{
	//release the fd and message
	if (msg->fd_to >= 0) {
		close(msg->fd_to);
		poll_del(msg->fd_to);
	}

	assert(msg->fd_from >= 0);
	if (poll_del(msg->fd_from) < 0)
		tiny_error("%s", "poll_del error");
	if (close(msg->fd_from) < 0)
		tiny_error("%s", "close socket error");

	free(msg);
}

void
read_ioerror(struct tiny_msg *msg)
{
	ssize_t bufsize;
	char *buf;

	switch (errno) {
		case ECONNRESET:
			tiny_notice("unexpected reset");
			closesock(msg);
			break;
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			//if blocked, save the received data to buffer for next read
			debug("blocked request");
			bufsize = tiny_bufsize();
			if (bufsize < 0) 
				tiny_error("tiny_bufsize error");

			buf = malloc(bufsize);
			tiny_clearbuf(buf, bufsize);

			msg->buf = buf;
			msg->bufsize = bufsize;
			break;
		default:
			tiny_error("read socket error");
	}
}

static void
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

