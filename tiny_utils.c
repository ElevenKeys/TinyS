#include <errno.h>
#include <unistd.h>

#include "tiny_log.h"
#include "tiny_config.h"
#include "tiny_poll.h"

void 
check_ioerror()
{
	if (errno == ECONNRESET) {
		tiny_notice("unexpected reset");
	} else {
		tiny_error("read socket error");
	}
}

void
closeboth(struct tiny_msg *msg)
{
	if (msg->fd_from > 0) {
		close(msg->fd_from);
		msg->fd_from = -1;
		poll_del(msg->fd_from);
	}

	if (msg->fd_to > 0) {
		close(msg->fd_to);
		msg->fd_to = -1;
		poll_del(msg->fd_to);
	}
}

