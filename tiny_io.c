#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#include "tiny_defs.h"
#include "tiny_io.h"
#include "tiny_log.h"

#define MIN(a,b) (a<b?a:b)
#define MAX(a,b) (a>b?a:b)

static pthread_key_t pt_key;
static pthread_once_t pt_once = PTHREAD_ONCE_INIT;

typedef struct
{
	char buffer[BUFFSIZE];
	char *bufptr;
	size_t cnt;
} io_buffer;

static void
create_key()
{
	pthread_key_create(&pt_key, NULL);
}

static int
initio(io_buffer **innebuf)
{
	void *ptr;

	pthread_once(&pt_once, create_key);

	if ( (*innebuf = (io_buffer *)pthread_getspecific(pt_key)) == NULL) {
		if ( (ptr = calloc(1, sizeof(io_buffer))) == NULL)
			return -1;	
		pthread_setspecific(pt_key, ptr);
		*innebuf = (io_buffer *)ptr;
	}
	return 0;
}

static ssize_t
fillbuff(io_buffer *innebuf, int fd)
{
	ssize_t len;
	
	while (innebuf->cnt <= 0) {
		len = read(fd, innebuf->buffer, BUFFSIZE);
		if (len < 0) {
			switch (errno) {
				case EINTR:
					continue;
//				case EAGAIN:
//#if EAGAIN != EWOULDBLOCK
//				case EWOULDBLOCK:
//#endif
//				return 0;
				default:
					return -1;
			}
		}
		else if (len == 0)
			return 0;
		else {
			innebuf->cnt = len;
			innebuf->bufptr = innebuf->buffer;
		}
	}

	return innebuf->cnt;
}

static ssize_t
buffread(io_buffer *innebuf, int fd, char *buf, size_t cnt)
{
	ssize_t ret, n;
	if ( (ret = fillbuff(innebuf, fd)) < 0)
		return ret;

	n = MIN(cnt, innebuf->cnt);
	memcpy(buf, innebuf->bufptr, n);
	innebuf->bufptr += n;
	innebuf->cnt -= n;
	return n;
}

static ssize_t
buffpeek(io_buffer *innebuf, int fd, char *buf, size_t start, size_t cnt)
{
	ssize_t ret, n;
	if ( (ret = fillbuff(innebuf, fd)) < 0)
		return ret;

	if (innebuf->cnt <= start)
		return 0;

	n = MIN(cnt, innebuf->cnt - start);
	memcpy(buf, innebuf->bufptr + start, n);
	return n;
}

ssize_t
tiny_bufsize()
{
	io_buffer *innebuf;

	if (initio(&innebuf) < 0)
		return -1;

	return innebuf->cnt;
}

int
tiny_clearbuf(char *buf, size_t cnt)
{
	io_buffer *innebuf;

	if (initio(&innebuf) < 0)
		return -1;

	if (buf != NULL && cnt != 0)
		memcpy(buf, innebuf->bufptr, MIN(cnt, innebuf->cnt));

	innebuf->bufptr = innebuf->buffer;
	innebuf->cnt = 0;
	
	return 0;
}

int
tiny_initbuf(char *buf, size_t cnt)
{
	io_buffer *innebuf;
	size_t size = 0;

	if (initio(&innebuf) < 0)
		return -1;

	if (buf != NULL && cnt != 0) {
		size = MIN(cnt, BUFFSIZE);
		memcpy(innebuf->buffer, buf, size);
	}

	innebuf->bufptr = innebuf->buffer;
	innebuf->cnt = size;
	return 0;
}

ssize_t
tiny_readn(int fd, char *buf, size_t len)
{
	io_buffer *innebuf;
	size_t n_left = len;
	ssize_t n_read;

	if (initio(&innebuf) < 0)
		return -1;

	while (n_left > 0) {
		n_read = buffread(innebuf, fd, buf, n_left);
		if (n_read < 0)
			return -1;
		else if (n_read == 0)
			break;
		else {
			buf += n_read;
			n_left -= n_read;
		}
	}

	return len - n_left;
}

ssize_t
tiny_readline(int fd, char *buf, size_t maxsize, int endline_type)
{
	io_buffer *innebuf;
	size_t n;
	ssize_t n_read;
	char flag, *bufp = buf;

	if (initio(&innebuf) < 0)
		return -1;
	
	if (endline_type == CR)
		flag = '\r';
	else
		flag = '\n';

	for (n = 1; n < maxsize; n++, bufp++) {
		if ( (n_read = buffread(innebuf, fd, bufp, 1)) == 1) {
			if (*bufp == flag){
				if(endline_type != CRLF)
					break;
				else if (n >= 2 && *(bufp - 1) == '\r')
					break;
			}
		} else if (n_read == 0) {
			if (n == 1)
				return 0;
			else
				break;
		} else
			return -1;
	}

	*++bufp = 0;
//#ifdef DEBUG
	//debug("readline :%s", buf);
//#endif
	return n;
}

ssize_t
tiny_peekline(int fd, char *buf, size_t maxsize, int endline_type)
{
	io_buffer *innebuf;
	size_t n, iterator = 0;
	ssize_t n_read;
	char flag, *bufp = buf;

	if (initio(&innebuf) < 0)
		return -1;
	
	if (endline_type == CR)
		flag = '\r';
	else
		flag = '\n';

	for (n = 1; n < maxsize; n++, bufp++, iterator++) {
		if ( (n_read = buffpeek(innebuf, fd, bufp, iterator, 1)) == 1) {
			if (*bufp == flag){
				if(endline_type != CRLF)
					break;
				else if (n >= 2 && *(bufp - 1) == '\r')
					break;
			}
		} else if (n_read == 0) {
			if (n == 1)
				return 0;
			else
				break;
		} else
			return -1;
	}

//#ifdef DEBUG
	//*++bufp = 0;
	//debug("peekline :%s", buf);
//#endif
	return n;
}

ssize_t
tiny_writen(int fd, const void * buff, size_t n)
{
	size_t n_left = n;
	ssize_t n_writen;

	while (n_left > 0) {
		n_writen = write(fd, buff, n_left);
		if (n_writen < 0) {
			if(errno == EINTR)
				continue;
			else
				return -1;
		}
		n_left -= n_writen;
		buff += n_writen;
	}
	return n;
}
