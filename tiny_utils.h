#ifndef TINY_UTILS_H
#define TINY_UTILS_H

void closeread(connect_ctx *ctx);
void closewrite(connect_ctx *ctx);
void close(connect_ctx *ctx);
ssize_t read_wrapper(connect_ctx *ctx);
void write_wrapper(connect_ctx *ctx, char *buf, size_t size);
void clienterror(int fd, int errnum, char *errmsg);

static inline void
state_transfer(connect_ctx *ctx, state_transfer_t *handler)
{
    ctx->hanlder = handler;
}

#define IO_CLOSE 0
#define IO_BLOCK -1
#define IO_ERROR -2
#define IO_BUFFERED -3

#endif
