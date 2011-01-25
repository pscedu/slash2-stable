/* $Id$ */

#include <pthread.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = pthread_rwlock_timedrdlock;
	exit(0);
}
