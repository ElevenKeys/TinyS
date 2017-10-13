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

static void
getminetype(char *filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".css"))
		strcpy(filetype, "text/css");
	else if(strstr(filename, ".js"))
		strcpy(filetype, "text/javascript");
	else if(strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if(strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else if(strstr(filename, ".png"))
		strcpy(filetype, "image/png");
	else
		strcpy(filetype, "text/plain");
}

static int
readfile(int fd, char *filename)
{
	int filefd;
	void *filep;
	struct stat sbuf;
	char buf[BUFFSIZE], filetype[MAXLINE];
	//time_t ticks;

	if (stat(filename, &sbuf) < 0) {
		clienterror(fd, 404, "Not found");
		return -1;
	}

	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
		clienterror(fd, 403, "Forbidden");
		return -1;
	}

	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	sprintf(buf, "%sServer: TinyS\r\n", buf);
	//ticks = time(NULL);
	//sprintf(buf, "%sDate: %s\r\n", buf, ctime(&ticks));
	sprintf(buf, "%sContent-Length: %ld\r\n", buf, sbuf.st_size);
	getminetype(filename, filetype);
	sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

	if (tiny_writen(fd, buf, strlen(buf)) < 0)
		tiny_error("write socket error");

	filefd = open(filename, O_RDONLY, 0);
	if (filefd < 0)
		tiny_error("open file error");

	filep = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, filefd, 0);
	if (filep < 0)
		tiny_error("mmap error");

	if (close(filefd) < 0)
		tiny_error("close file error");

	if (tiny_writen(fd, filep, sbuf.st_size) < 0) {
		tiny_error("write socket error");
	}

	if (munmap(filep, sbuf.st_size) < 0)
		tiny_error("munmap error");

	return 0;
}
