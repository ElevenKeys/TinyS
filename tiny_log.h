#ifndef TINY_LOG_H
#define TINY_LOG_H

#include <stdbool.h>

void init_logger(char *id, bool daemon_proc);
void tiny_error(char *msg, ...);
void tiny_notice(char *msg, ...);

#ifdef DEBUG
#define debug(msg, ...) tiny_notice(msg, ##__VA_ARGS__)
#else
#define debug(msg, ...)
#endif

#endif
