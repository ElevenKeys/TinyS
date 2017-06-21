#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "tiny_config.h"
#include "tiny_worker.h"
#include "tiny_log.h"
#include "tiny_poll.h"
#include "tiny_socket.h"
#include "tiny_mq.h"

static int init_config(struct tiny_config*);
static tiny_context c =
{
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER,
	SPINLOCK_INITIALIZER
};

int
main(int argc, char **argv)
{
	struct tiny_config *config = &c.config;
	int listenfd, connfd, n;
	struct tiny_msg *msgs[1024], *msg;
	struct sockaddr_storage addr;
	socklen_t len;

	if (init_config(config) < 0) {
		fprintf(stderr, "%s\n", "fail to read the configuration file");
		exit(-1);
	}

	init_logger("tinyserver", config->daemon);
	tiny_mq_init();
	poll_create();

	if (config->daemon) {
		if (daemon(0,0) < 0)
			tiny_error("daemon error");
	}

	pthread_t pid[config->thread];
	for (int i = 0; i < config->thread; i++) {
		if (pthread_create(pid + i, NULL, routine, &c) < 0)
			tiny_error("create thread error");
	}

	listenfd = tcp_listen(config->agent_addr, config->agent_port, NULL);
	poll_add(listenfd, NULL);

	while (1) {
		n = poll_wait((void **)&msgs, 1024);
		for (int i = 0; i < n; i++) {
			if (msgs[i] == NULL) {
				len = sizeof(addr);
				connfd = accept(listenfd, (SA*)&addr, &len);
				if (connfd < 0)
					tiny_error("accept error");

				debug("connect from %s", sock_ntop((SA*)&addr, len));
				
				msg = (struct tiny_msg*)malloc(sizeof(struct tiny_msg));
				msg->type = FORWARD;
				msg->fd_from = connfd;
				msg->fd_to = -1;
				msg->buf = NULL;
				msg->use = false;

				poll_add(connfd, msg);
			} else {
				tiny_mq_push(msgs[i]);

				debug("dispatch message:type is %s, socket is %d", msgs[i]->type == FORWARD?"forward":"response", msgs[i]->fd_from);

				if(c.sleep > 0)
					pthread_cond_signal(&c.cond);
			}
		}
	}
}

static int init_config(struct tiny_config *config)
{
	config->daemon = false;
	config->thread = 4;
	strcpy(config->agent_addr, "127.0.0.1");
	strcpy(config->agent_port, "80");
	config->appserver_n = 2;
	config->appserver = calloc(2, sizeof(struct proxy_server));
	config->appserver[0].param_pass = PROXY;
	strcpy(config->appserver[0].app_addr, "www.bilibili.com");
	strcpy(config->appserver[0].app_port, "http");
	config->appserver[1].param_pass = PROXY;
	strcpy(config->appserver[1].app_addr, "www.bilibili.com");
	strcpy(config->appserver[1].app_port, "http");
	return 0;
}
