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
#include "tiny_utils.h"

/*macro*/
#define CONTENT_LENGTH  "Content-Length"
#define TRANSFER_ENCODING  "Transfer-Encoding"
#define HOST "Host"
#define CHUNKED "chunked"

/*Function prototype*/
static void request_line_handler(connect_ctx*);
static void request_header_handler(connect_ctx*);
static void request_body_handler(connect_ctx*);
static void upstream_line_handler(connect_ctx*);
static void upstream_header_handler(connect_ctx*);
static void upstream_body_handler(connect_ctx*);
static void upstream_body_length_handler(connect_ctx*);
static void upstream_body_chunked_handler(connect_ctx*);

module_t tiny_proxy_module = {
	request_line_handler,
	upstream_line_handler,
};

static void 
request_line_handler(connect_ctx *ctx)
{
    char buf[MAXLINE];
    ssize_t readcnt;

	readcnt = readline_wrapper(ctx, buf, sizeof(buf));
	if (readcnt == 0) {
        //socket is closed by peer.
        closeread(ctx);
		return;
    } else if (readcnt < 0) {
        //socket is blocked, waiting for the next handling.
        return;
    } else {
        //forward the request line.
        writen_wrapper(ctx->peer_connect, buf, readcnt);
        state_transfer(ctx, request_header_handler);
    }
}

static void
request_header_handler(connect_ctx *ctx)
{
    char buf[MAXLINE];
    ssize_t readcnt;

    while (1) {
        readcnt = readline_wrapper(ctx, buf, sizeof(buf));
        if (readcnt == 0) {
            //socket is unexpectedly closed by peer.
            close(ctx);
            tiny_notice("Unexpected close when reading request header")
            return;
        } else if (readcnt < 0) {
            //socket is blocked, waiting for the next handling.
            return;
        } else {
            //forward the request header.
            if (strcmp(buf, "\r\n") == 0) {
                //The end of request header
                writen_wrapper(ctx->peer_connect, "\r\n", strlen("\r\n"));
                state_transfer(ctx, request_body_hanlder);
                return;
            }

            split = strchr(buf, ':');
            if (split == NULL) {
                tiny_notice("HTTP header not recognized, %s", buf);
                continue;
            }

            //skip whitespace
            while (*++split == ' ')
                ;

            if (strncmp(buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
                ctx->body_left = strtoul(split, NULL, 10);
            }

            //replace the host
            if (strncmp(buf, HOST, strlen(HOST)) == 0) {
                sprintf(split, "%s\r\n", ctx->host);
                readcnt = strlen(buf);
            }

            writen_wrapper(ctx->peer_connect, buf, readcnt);
        }
    }
}

static void 
request_body_handler(connect_ctx *ctx)
{
    char buf[BUFFSIZE];
    size_t toberead;
    ssize_t readcnt;

    while (ctx->body_left > 0) {
        toberead = ctx->body_left < sizeof(buf)? ctx->body_left: sizeof(buf);
        readcnt = readn_wrapper(ctx, buf, toberead);
        if (readcnt == 0) {
            //socket is unexpectedly closed by peer.
            close(ctx);
            tiny_notice("Unexpected close when reading request body")
            return;
        } else if (readcnt < 0) {
            //socket is blocked, waiting for the next handling.
            return;
        } else {
            //forward the request body.
            writen_wrapper(ctx->peer_connect, buf, readcnt);
            ctx->body_left -= readcnt;
        }
    }
    state_transfer(ctx, request_line_handler);
}

static void 
upstream_line_handler(connect_ctx *ctx)
{
    char buf[MAXLINE];
    ssize_t readcnt;

	readcnt = readline_wrapper(ctx, buf, sizeof(buf));
	if (readcnt == 0) {
        //socket is closed by peer.
        closeread(ctx);
		return;
    } else if (readcnt < 0) {
        //socket is blocked, waiting for the next handling.
        return;
    } else {
        //forward the upstream line.
        writen_wrapper(ctx->peer_connect, buf, readcnt);
        state_transfer(ctx, upstream_header_handler);
    }
}

static void
upstream_header_handler(connect_ctx *ctx)
{
    char buf[MAXLINE];
    ssize_t readcnt;

    while (1) {
        readcnt = readline_wrapper(ctx, buf, sizeof(buf));
        if (readcnt == 0) {
            //socket is unexpectedly closed by peer.
            close(ctx);
            tiny_notice("Unexpected close when reading upstream header")
            return;
        } else if (readcnt < 0) {
            //socket is blocked, waiting for the next handling.
            return;
        } else {
            //forward the upstream header.
            if (strcmp(buf, "\r\n") == 0) {
                //The end of request header
                writen_wrapper(ctx->peer_connect, "\r\n", strlen("\r\n"));
                state_transfer(ctx, upstream_body_handler);
                return;
            }

            split = strchr(buf, ':');
            if (split == NULL) {
                tiny_notice("HTTP header not recognized: %s", buf);
                continue;
            }

            //skip whitespace
            while (*++split == ' ')
                ;

            if (ctx->body_type == BODY_TYPE_NONE) {
                //get Content-Length head
                if (strncmp(buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
                    ctx->body_left = strtoul(split, NULL, 10);
                    ctx->body_type = BODY_TYPE_LENGTH;
                }

                //get Transfer-Encoding head
                if (strncmp(buf, TRANSFER_ENCODING, strlen(TRANSFER_ENCODING)) == 0 &&
                    strncmp(split, CHUNKED, strlen(CHUNKED)) == 0) {
                    ctx->body_type = BODY_TYPE_CHUNKED;
                }
            }

            writen_wrapper(ctx->peer_connect, buf, readcnt);
        }
    }
}

static void 
upstream_body_handler(connect_ctx *ctx)
{
    assert(ctx->body_type != BODY_TYPE_NONE);

    if (ctx->body_type == BODY_TYPE_LENGTH)
        state_transfer(ctx, upstream_body_length_handler);
    else
        state_transfer(ctx, upstream_body_chunked_handler);
}

static void 
upstream_body_length_handler(connect_ctx *ctx)
{
    char buf[BUFFSIZE];
    size_t toberead;
    ssize_t readcnt;

    while (ctx->body_left > 0) {
        toberead = ctx->body_left < sizeof(buf)? ctx->body_left: sizeof(buf);
        readcnt = readn_wrapper(ctx, buf, toberead);
        if (readcnt == 0) {
            //socket is unexpectedly closed by peer.
            close(ctx);
            tiny_notice("Unexpected close when reading upstream body")
            return;
        } else if (readcnt < 0) {
            //socket is blocked, waiting for the next handling.
            return;
        } else {
            //forward the upstream body.
            writen_wrapper(ctx->peer_connect, buf, readcnt);
            ctx->body_left -= readcnt;
        }
    }
    state_transfer(ctx, upstream_line_handler);
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
            writen_wrapper(ctx->peer_connect, buf, readcnt);
            ctx->body_left -= readcnt;
        }
    }
    state_trans((state_transfer_t*)(&tiny_proxy_module), ctx, PHRASE_REQUEST_LINE);
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

	}

}

