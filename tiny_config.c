#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
struct spinlock g_spinlock = SPINLOCK_INITIALIZER;
struct tiny_config g_config;
int g_sleep;
