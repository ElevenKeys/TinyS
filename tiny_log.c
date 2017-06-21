#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>

#include "tiny_defs.h"

static bool daemon;

void
init_logger(char *id, bool daemon_proc)
{
	openlog(id, 0, LOG_LOCAL1);
	daemon = daemon_proc;
}

void
tiny_error(char *msg, ...)
{
	size_t len;
	char buff[MAXLINE];
	va_list ap;
	time_t ticks;

	ticks = time(NULL);
	snprintf(buff, sizeof(buff), "%.24s : %s\n", ctime(&ticks), strerror(errno));
	len = strlen(buff);

	va_start(ap, msg);
	vsnprintf(buff + len, sizeof(buff) - len, msg, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", buff);
	if (daemon)
		syslog(LOG_ERR, buff);

	exit(-1);
}

void
tiny_notice(char *msg, ...)
{
	size_t len;
	char buff[MAXLINE];
	va_list ap;
	time_t ticks;

	ticks = time(NULL);
	snprintf(buff, sizeof(buff), "%.24s\n", ctime(&ticks));
	len = strlen(buff);

	va_start(ap, msg);
	vsnprintf(buff + len, sizeof(buff) - len, msg, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", buff);
	if (daemon)
		syslog(LOG_NOTICE, buff);
}
