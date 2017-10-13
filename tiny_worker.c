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



static bool
get_local(int sockfd,char *request)
{
	char method[16], version[16], url[MAXLINE], filename[MAXLINE] = ".";
	
	sscanf(request, "%s %s %s", method, url, version);
	
	if (strcasecmp(method, "GET"))
		return false;

	if (strstr(url, "cgi-bin"))
		return false;

	strcat(filename, url);

	if (!strcmp(filename,"./"))
		strcpy(filename, "./index.html");

	readfile(sockfd, filename);

	return true;
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
			msg->buf = NULL;
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

            if (get_local(msg->fd_from, request)) {
                closesock(msg);
                goto end;
            }

			dispatch(msg);
			pass_handler_set[msg->param_pass]->request(msg);
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

