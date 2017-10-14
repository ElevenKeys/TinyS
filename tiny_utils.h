#ifndef TINY_UTILS_H
#define TINY_UTILS_H

void closeread(connect_ctx *ctx);
void closewrite(connect_ctx *ctx);
int readline_wrapper(connect_ctx *ctx, char *buf, size_t size);
int readn_wrapper(connect_ctx *ctx, char *buf, size_t toberead);
void writen_wrapper(connect_ctx *ctx, char *buf, size_t size);
void clienterror(int fd, int errnum, char *errmsg);

static inline void
state_transfer(connect_ctx *ctx, state_transfer_t *handler)
{
    ctx->hanlder = handler;
    handler(ctx);
}

#endif
