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
static bool request_line_handler(connect_ctx*);
static bool request_header_handler(connect_ctx*);
static bool request_body_handler(connect_ctx*);
static bool upstream_line_handler(connect_ctx*);
static bool upstream_header_handler(connect_ctx*);
static bool upstream_body_handler(connect_ctx*);
static bool upstream_body_length_handler(connect_ctx*);
static bool upstream_body_chunked_header_handler(connect_ctx*);
static bool upstream_body_chunked_body_handler(connect_ctx*);
static bool upstream_body_chunked_footer_handler(connect_ctx*);

static int loop_write(connect_ctx*, char*);

module_t tiny_proxy_module = {
	request_line_handler,
	upstream_line_handler,
};

static bool 
request_line_handler(connect_ctx *ctx)
{
    ssize_t readcnt;

    while (true) {
        readcnt = read_wrapper(ctx);
        if (readcnt == IO_CLOSE) {
            //socket is closed by peer.
            closeread(ctx);
            return false;
        } else if (readcnt == IO_BLOCK) {
            //socket is blocked, waiting for the next handling.
            return false;
        } else if (readcnt == IO_ERROR) {
            //socket error
            read_ioerror(ctx);
            return false;
        }
        
        //read request line until CRLF
        while (ctx->ioend != ctx->iostart &&
            ctx->bufend - ctx->buf < MAXLINE &&
            *(ctx->bufend-1) != '\n') {
            *ctx->bufend++ = *ctx->iostart++;
        }

        //request line is too long
        if(ctx->bufend - ctx->buf >= MAXLINE) {
            clienterror(ctx->fd, 414, "Request-URL Too Long");
            close(ctx);
            tiny_notice("request line too long");
            return false;
        }

        if (*(ctx->bufend-1) != '\n')
            continue;
            
        //forward the request line
        write_wrapper(ctx->peer_connect, ctx->buf, ctx->bufend - ctx->buf);
        ctx->bufend = ctx->buf;
        state_transfer(ctx, request_header_handler);
        return true;
    }
}

static bool
request_header_handler(connect_ctx *ctx)
{
    ssize_t readcnt;
    char *split;

    while (true) {
        readcnt = read_wrapper(ctx);
        if (readcnt <= 0)
            return false;

        //read request line until CRLF
        while (ctx->ioend != ctx->iostart &&
            ctx->bufend - ctx->buf < MAXLINE &&
            *(ctx->bufend-1) != '\n') {
            *ctx->bufend++ = *ctx->iostart++;
        }

        //request header is too long
        if(ctx->bufend - ctx->buf >= MAXLINE) {
            clienterror(ctx->fd, 400, "Request too long");
            close(ctx);
            tiny_notice("request header too long");
            return false;
        }

        if (*(ctx->bufend-1) != '\n')
            continue;

        *ctx->bufend = '\0';

        //forward the request header.
        if (strncmp(ctx->buf, "\r\n", 2) == 0) {
            //The end of request header
            write_wrapper(ctx->peer_connect, "\r\n", 2);
            ctx->bufend = ctx->buf;
            state_transfer(ctx, request_body_hanlder);
            return true;
        }

        split = strchr(ctx->buf, ':');
        if (split == NULL) {
            tiny_notice("HTTP header cannot be recognized\n %s", buf);
            continue;
        }

        //skip whitespace
        while (*++split == ' ')
            ;

        if (strncmp(ctx->buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
            ctx->body_left = strtoul(split, NULL, 10);
        }

        //replace the host
        if (strncmp(ctx->buf, HOST, strlen(HOST)) == 0) {
            sprintf(split, "%s\r\n", ctx->host);
            readcnt = strlen(buf);
        }

        write_wrapper(ctx->peer_connect, ctx->buf, ctx->bufend - ctx->buf);
        ctx->ctxend = ctx->buf;
    }

    //Cannot touch
    assert(false);
    return false;
}

static bool 
request_body_handler(connect_ctx *ctx)
{
    char *closmsg = "Unexpected close when reading request body";
    int ret = loop_write(ctx, closmsg);
    if (!ret) {
        state_transfer(ctx, request_line_handler);
        return true;
    } else {
        return false;
    }
}

static bool 
upstream_line_handler(connect_ctx *ctx)
{
    ssize_t readcnt;

	readcnt = read_wrapper(ctx);
    if (readcnt <= 0)
        return false;

    //read upstream line until CRLF
    while (ctx->ioend != ctx->iostart &&
        *(ctx->bufend-1) != '\n') {
        *ctx->bufend++ = *ctx->iostart++;
    }

    if (*(ctx->bufend-1) != '\n')
        continue;
        
    //forward the upstream line
    write_wrapper(ctx->peer_connect, ctx->buf, ctx->bufend - ctx->buf);
    ctx->bufend = ctx->buf;
    state_transfer(ctx, upstream_header_handler);
    return true;
}

static bool
upstream_header_handler(connect_ctx *ctx)
{
    ssize_t readcnt;
    char *split;

    while (true) {
        readcnt = read_wrapper(ctx);
        if (readcnt <= 0)
            return false;

        //read upstream line until CRLF
        while (ctx->ioend != ctx->iostart &&
            *(ctx->bufend-1) != '\n') {
            *ctx->bufend++ = *ctx->iostart++;
        }

        if (*(ctx->bufend-1) != '\n')
            continue;

        *ctx->bufend = '\0';

        //forward the upstream header.
        if (strcmp(ctx->buf, "\r\n") == 0) {
            //The end of request header
            write_wrapper(ctx->peer_connect, "\r\n", 2);
            ctx->bufend = ctx->buf;
            state_transfer(ctx, upstream_body_handler);
            return true;
        }

        split = strchr(ctx->buf, ':');

        //skip whitespace
        while (*++split == ' ')
            ;

        if (ctx->body_type == BODY_TYPE_NONE) {
            //get Content-Length head
            if (strncmp(ctx->buf, CONTENT_LENGTH, strlen(CONTENT_LENGTH)) == 0) {
                ctx->body_left = strtoul(split, NULL, 10);
                ctx->body_type = BODY_TYPE_LENGTH;
            }

            //get Transfer-Encoding head
            if (strncmp(ctx->buf, TRANSFER_ENCODING, strlen(TRANSFER_ENCODING)) == 0 &&
                strncmp(split, CHUNKED, strlen(CHUNKED)) == 0) {
                ctx->body_type = BODY_TYPE_CHUNKED;
            }
        }

        write_wrapper(ctx->peer_connect, ctx->buf, ctx->bufend - ctx->buf);
        ctx->bufend = ctx->buf;
    }

    //Cannot touch
    assert(false);
    return false;
}

static bool 
upstream_body_handler(connect_ctx *ctx)
{
    assert(ctx->body_type != BODY_TYPE_NONE);

    if (ctx->body_type == BODY_TYPE_LENGTH)
        state_transfer(ctx, upstream_body_length_handler);
    else
        state_transfer(ctx, upstream_body_chunked_header_handler);
    return true;
}

static bool 
upstream_body_length_handler(connect_ctx *ctx)
{
    char *closmsg = "Unexpected close when reading upstream body";
    int ret = loop_write(ctx, closmsg);
    if (!ret) {
        state_transfer(ctx, upstream_line_handler);
        return true;
    } else {
        return false;
    }
}

static bool
upstream_body_trunked_header_handler(connect_ctx *ctx)
{
    ssize_t readcnt;

    readcnt = read_wrapper(ctx);
    if (readcnt == IO_CLOSE) {
        //socket is unexpectedly closed by peer.
        close(ctx);
        tiny_notice("Unexpected close when reading the chunked header of upstream body")
        return false;
    } else if (readcnt == IO_BLOCK) {
        //socket is blocked, waiting for the next handling.
        return false;
    } else {
        writen_wrapper(ctx->peer_connect, buf, readcnt);
        ctx->body_left = strtoul(buf, NULL, 16);
        if (ctx->body_left > 0)
            state_transfer(ctx, upstream_body_trunked_body_handler);
        else
            state_transfer(ctx, upstream_body_trunked_footer_handler);
        return true;
    }
}


static bool
upstream_body_trunked_body_handler(connect_ctx *ctx)
{
    char *closmsg = "Unexpected close when reading the chunked body of upstream body";
    int ret = loop_write(ctx, closmsg);
    if (!ret) {
        state_transfer(ctx, upstream_body_chunked_header_handler);
        return true;
    } else {
        return false;
    }
}

static bool
upstream_body_trunked_spliter_handler(connect_ctx *ctx)
{
    char buf[2];
    ssize_t readcnt;

    readcnt = readn_wrapper(msg, msg->fd_to, buf, 2);
        if (readcnt == 0) {
            //socket is unexpectedly closed by peer.
            close(ctx);
            tiny_notice("Unexpected close when reading the chunked body of upstream body")
            return false;
        } else if (readcnt < 0) {
            //socket is blocked, waiting for the next handling.
            return false;
        } else {
            //forward the upstream body.
            if (strncmp(buf, "\r\n", 2) != 0) {
                //bad format
                close(ctx);
                tiny_notice("parse chunked response error");
                return false;
            }

            writen_wrapper(ctx->peer_connect, buf, readcnt);
        }

    if (readcnt <= 0) {
        return NULL;
    }

}
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

static int
loop_write(connect_ctx *ctx, char *closmsg)
{
    char buf[BUFFSIZE];
    ssize_t readcnt;

    while (ctx->body_left > 0) {
        readcnt = read_wrapper(ctx);
        if (readcnt <= 0)
            return -1;

        if (readcnt > ctx_body_left) {
            clienterror(ctx->fd, 400, "Bad Request");
            close(ctx);
            tiny_notice("bad chunked format");
            return -1;
        }

        //forward the upstream body.
        write_wrapper(ctx->peer_connect, ctx->iostart, readcnt);
        ctx->iostart += readcnt;
        ctx->body_left -= readcnt;
    }
    return 0;
}
