#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "tiny_config.h"
#include "tiny_worker.h"
#include "tiny_log.h"
#include "tiny_poll.h"
#include "tiny_socket.h"
#include "tiny_mq.h"

/*Function prototype*/
typedef void sigfunc(int);
static sigfunc* signal_(int signo, sigfunc *func);
static void sig_handler(int signo);
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
	connect_ctx *ctxs[1024], *ctx;
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

	if (signal_(SIGPIPE, sig_handler) == SIG_ERR)
		tiny_error("signal error");

	pthread_t pid[config->thread];
	for (int i = 0; i < config->thread; i++) {
		if (pthread_create(pid + i, NULL, routine, &c) < 0)
			tiny_error("create thread error");
	}

	listenfd = tcp_listen(config->agent_addr, config->agent_port, NULL);
	poll_add(listenfd, NULL);

	while (1) {
		n = poll_wait((void **)&ctxs, 1024);
		for (int i = 0; i < n; i++) {
			if (ctxs[i] == NULL) {
				len = sizeof(addr);
				while ((connfd = accept(listenfd, (SA*)&addr, &len)) >= 0) {
					debug("connect from %s, new fd is %d", sock_ntop((SA*)&addr, len), connfd);

					create_conn_ctx(&ctx, connfd, PHRASE_REQUEST_LINE);

					poll_add(connfd, ctx);
				}
				if (connfd < 0) {
					switch (errno) {
						case EAGAIN:
#if EAGAIN != EWOULDBLOCK
						case EWOULDBLOCK:
#endif
							continue;
						default:
							tiny_error("accept error");
					}
				}

			} else {
				tiny_mq_push(ctxs[i]);

				if(c.sleep > 0)
					pthread_cond_broadcast(&c.cond);
			}
		}
	}
}

static sigfunc* signal_(int signo, sigfunc *func)
{
	struct sigaction act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	} else {
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif
	}

	if (sigaction(signo, &act, &oact) < 0)
		return SIG_ERR;
	return oact.sa_handler;
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
	strcpy(config->appserver[0].app_addr, "www.dilidili.wang");
	strcpy(config->appserver[0].app_port, "http");
	config->appserver[1].param_pass = PROXY;
	strcpy(config->appserver[1].app_addr, "www.dilidili.wang");
	strcpy(config->appserver[1].app_port, "http");
	return 0;
}

static void sig_handler(int signo)
{
	tiny_notice("received signal %d",signo); 
}
