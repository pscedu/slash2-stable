/* $Id$ */

#include <pthread.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = pthread_mutex_timedlock;
	exit(0);
}
