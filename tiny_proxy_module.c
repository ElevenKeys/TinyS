#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "tiny_config.h"
#include "tiny_io.h"
#include "tiny_log.h"
#include "tiny_worker.h"
#include "tiny_defs.h"

#define CONTENT_LENGTH  "Content-Length"
#define TRANSFER_ENCODING  "Transfer-Encoding"
#define HOST "Host"
#define CHUNKED "chunked"

static int
readline_wrapper(struct tiny_msg *msg, int fd, char *buf, size_t size)
{
	ssize_t readcnt;

	readcnt = tiny_readline(fd, buf, size, CRLF);
	if (readcnt < 0) {
		read_ioerror(msg);
		return -1;
	} else if (readcnt == 0) {
		closesock(msg);
		return 0;
	}

	return readcnt;
}

static int
readn_wrapper(struct tiny_msg *msg, int fd, char *buf, size_t toberead)
{
	ssize_t readcnt;

	readcnt = tiny_readn(fd, buf, toberead);
	if (readcnt < 0) {
		read_ioerror(msg);
		return -1;
	} else if (readcnt == 0) {
		closesock(msg);
		return 0;
	}

	return readcnt;
}

static inline void
writen_wrapper(int fd, char *buf, size_t size)
{
	if (tiny_writen(fd, buf, size) < 0)
		tiny_error("write socket error");
}

static int
loop_write(struct tiny_msg *msg, int fd_r, int fd_w, size_t len)
{
	ssize_t readcnt;
	size_t toberead;
	char buf[BUFFSIZE];

	while (len > 0) {
		toberead = len < sizeof(buf)? len: sizeof(buf);
		readcnt = readn_wrapper(msg, fd_r, buf, toberead);
		if (readcnt <= 0) {
			return -1;
		}

		writen_wrapper(fd_w, buf, readcnt);
		len -= readcnt;
	}
	return 0;
}

static void*
proxy_request_handler(struct tiny_msg *msg)
{
	assert(msg->fd_from > 0);
	assert(msg->fd_to > 0);

	char buf[BUFFSIZE], *split;
	ssize_t readcnt, len = 0;

	//forward request line
	readcnt = readline_wrapper(msg, msg->fd_from, buf, sizeof(buf));
	if (readcnt <= 0) {
		return NULL;
	}

	writen_wrapper(msg->fd_to, buf, readcnt);

	//forword request header
	while (1) {
		readcnt = readline_wrapper(msg, msg->fd_from, buf, sizeof(buf));
		if (readcnt <= 0) {
			return NULL;
		}

		if (strcmp(buf, "\r\n") == 0) {
			writen_wrapper(msg->fd_to, "\r\n", strlen("\r\n"));
			break;
		}

		split = strchr(buf, ':');
		if (split == NULL) {
			tiny_notice("HTTP header not recognized");
			closesock(msg);
			return NULL;
		}

		while (*++split == ' ')
			;

		if (strncmp(buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
			len = strtoul(split, NULL, 10);
		}

		//switch the host
		if (strncmp(buf, HOST, strlen(HOST)) == 0) {
			sprintf(split, "%s\r\n", msg->host );
			readcnt = strlen(buf);
		}

		writen_wrapper(msg->fd_to, buf, readcnt);
	}

	loop_write(msg, msg->fd_from, msg->fd_to, len);
	return NULL;
}

static void*
proxy_upstream_handler(struct tiny_msg *msg)
{
	assert(msg->fd_from > 0);
	assert(msg->fd_to > 0);

	char buf[BUFFSIZE], *split;
	bool len_flag = false,chunked_flag = false;
	ssize_t readcnt, toberead, len = 0;

	//forward response line
	readcnt = readline_wrapper(msg, msg->fd_to, buf, sizeof(buf));
	if (readcnt <= 0) {
		return NULL;
	}

	writen_wrapper(msg->fd_from, buf, readcnt);

	//forward response header
	while (1) {
		readcnt = readline_wrapper(msg, msg->fd_to, buf, sizeof(buf));
		if (readcnt <= 0) {
			return NULL;
		}

		writen_wrapper(msg->fd_from, buf, readcnt);

		if (strcmp(buf, "\r\n") == 0)
			break;

		split = strchr(buf, ':');
		if (split == NULL) {
			tiny_notice("HTTP header not recognized");
			closesock(msg);
			return NULL;
		}

		while (*++split == ' ')
			;

		if (!len_flag) {
			//get Content-Length head
			if (strncmp(buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
				len = strtoul(split, NULL, 10);
				len_flag = true;
			}

			//get Transfer-Encoding head
			if (strncmp(buf, TRANSFER_ENCODING, strlen(TRANSFER_ENCODING)) == 0 && strncmp(split, CHUNKED, strlen(CHUNKED)) == 0) {
				chunked_flag = true;
				len_flag = true;
			}
		}
	}

	//parse chunked body
	if (chunked_flag) {
		while (true) {
			readcnt = readline_wrapper(msg, msg->fd_to, buf, sizeof(buf));
			if (readcnt <= 0) {
				return NULL;
			}

			writen_wrapper(msg->fd_from, buf, readcnt);

			toberead = strtoul(buf, NULL, 16);
			if (loop_write(msg, msg->fd_to, msg->fd_from, toberead) < 0)
				tiny_error("parse chunked response error");

			readcnt = readn_wrapper(msg, msg->fd_to, buf, 2);
			if (readcnt <= 0) {
				return NULL;
			}

			if (strncmp(buf, "\r\n", 2) != 0)
				tiny_error("parse chunked response error");

			writen_wrapper(msg->fd_from, buf, readcnt);

			if(toberead == 0) {
				return NULL;
			}
		}
	} else {
		//parse fixed size body
		loop_write(msg, msg->fd_to, msg->fd_from, len);
		return NULL;
	}
}

handler_module_t tiny_proxy_module = {proxy_request_handler, proxy_upstream_handler};
