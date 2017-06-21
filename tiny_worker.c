#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include "tiny_config.h"
#include "tiny_log.h"
#include "tiny_defs.h"
#include "tiny_socket.h"
#include "tiny_io.h"
#include "tiny_mq.h"
#include "tiny_poll.h"

static tiny_context *g_ctx;

static void
clienterror(int fd, int errnum, char *errmsg)
{
	char buf[BUFFSIZE];

	sprintf(buf, "HTTP/1.1 %d %s\r\n", errnum, errmsg);
	sprintf(buf, "%s%s\r\n\r\n", buf, errmsg);
	tiny_writen(fd, buf, strlen(buf));
}

static void
getfiletype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".css"))
		strcpy(filetype, "text/css");
	else if(strstr(filename, ".js"))
		strcpy(filetype, "text/javascript");
	else if(strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if(strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else if(strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else
		strcpy(filetype, "text/plain");
}

static int
readfile(int fd, char *filename)
{
	int filefd;
	void *filep;
	struct stat sbuf;
	char buf[BUFFSIZE], filetype[MAXLINE];
	time_t ticks;

	if (stat(filename, &sbuf) < 0) {
		clienterror(fd, 404, "Not found");
		return -1;
	}

	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
		clienterror(fd, 403, "Forbidden");
		return -1;
	}

	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	sprintf(buf, "%sServer: TinyS\r\n", buf);
	ticks = time(NULL);
	/*sprintf(buf, "%sDate: %s\r\n", buf, ctime(&ticks));*/
	/*sprintf(buf, "%sContent-Length: %ld\r\n", buf, 0);*/
	sprintf(buf, "%sContent-Length: %ld\r\n", buf, sbuf.st_size);
	getfiletype(filename, filetype);
	sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

	if (tiny_writen(fd, buf, strlen(buf)) < 0)
		tiny_error("write socket error");

	filefd = open(filename, O_RDONLY, 0);
	if (filefd < 0)
		tiny_error("open file error");

	filep = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
	if (filep < 0)
		tiny_error("mmap error");

	close(filefd);

	if (tiny_writen(fd, filep, sbuf.st_size) < 0)
		tiny_error("write socket error");

	if (munmap(filep, sbuf.st_size) < 0)
		tiny_error("munmap error");

	return 0;
}

static bool
get_local(int sockfd,char *request)
{
	char method[16], version[16], url[MAXLINE], filename[MAXLINE] = ".";
	size_t tail;
	
	sscanf(request, "%s %s %s", method, url, version);
	
	if (strcasecmp(method, "GET"))
		return false;

	if (strstr(url, "cgi-bin"))
		return false;

	strcat(filename, url);

	tail = strlen(filename) - 1;
	if (filename[tail] == '/')
		filename[tail] = 0;
	
	if (!strcmp(filename,"."))
		strcpy(filename, "./index.html");

	readfile(sockfd, filename);

	return true;
}

static void
dispatch(struct tiny_msg *msg)
{
	//distribute the request according to the balacing strategy.
	static int index=0;
	int index_cp;
	struct proxy_server *app;

	msg->type = RESPONSE;

	//connect the client with appplication server
	if(msg->fd_to < 0) {
		spinlock_lock(&g_ctx->spinlock);
		index %= g_ctx->config.appserver_n; 
		index_cp = index++;
		spinlock_unlock(&g_ctx->spinlock);

		app = g_ctx->config.appserver + index_cp;
		msg->param_pass = app->param_pass;
		msg->host = app->app_addr;
		msg->fd_to = tcp_connect(app->app_addr, app->app_port);

		poll_add(msg->fd_to, msg);
	}
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
	close(msg->fd_from);
	poll_del(msg->fd_from);

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

void *
routine(void *arg)
{
	struct tiny_msg* msg;
	char request[MAXLINE];
	ssize_t readn;

	g_ctx = (tiny_context*)arg;

	for (;;) {
		if ((msg = tiny_mq_pop()) == NULL) {
			pthread_mutex_lock(&g_ctx->mutex);
			g_ctx->sleep++;
			pthread_cond_wait(&g_ctx->cond, &g_ctx->mutex);
			g_ctx->sleep--;
			pthread_mutex_unlock(&g_ctx->mutex);
			continue;
		} 

		debug("%d thread awake, fd %d, type %s", pthread_self(), msg->fd_from, msg->type == FORWARD?"FORWARD":"RESPONSE");
		//if there is buffer for last incomplete read,initialize the io with the buffer
		if (msg->buf != NULL) {
			tiny_initbuf(msg->buf, msg->bufsize);
			free(msg->buf);
		} else {
			tiny_initbuf(NULL, 0);
		}

		if (msg->type == FORWARD) {
			readn = tiny_peekline(msg->fd_from, request, MAXLINE, CRLF);
			if (readn < 0) {
				read_ioerror(msg);
				goto end;
			} else if (readn == 0) {
				closesock(msg);
				goto end;
			}

			//if (get_local(msg->fd_from, request)) {
				//goto end;
			//}

			dispatch(msg);
			pass_handler_set[msg->param_pass]->forward(msg);
		} else if (msg->type == RESPONSE) {
			pass_handler_set[msg->param_pass]->response(msg);
			msg->type = FORWARD;
		}

end:
		//release the use of the message
		spinlock_lock(&g_ctx->spinlock);
		msg->use = false;
		spinlock_unlock(&g_ctx->spinlock);
	}
} 

