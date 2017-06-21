#ifndef TINY_IO_H
#define TINY_IO_H

#include <stdlib.h>

#define CR 0
#define LF 1
#define CRLF 2 

ssize_t tiny_bufsize();
int tiny_clearbuf(char *buf, size_t cnt);
int tiny_initbuf(char *buf, size_t cnt);
ssize_t tiny_readn(int fd, char *buf, size_t len);
ssize_t tiny_readline(int fd, char *buf, size_t maxsize, int endline_type);
ssize_t tiny_peekline(int fd, char *buf, size_t maxsize, int endline_type);
ssize_t tiny_writen(int fd, const void * buff, size_t n);

#endif
