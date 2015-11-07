/* $Id$ */

#include <pthread.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *p;

	(void)argc;
	(void)argv;
	p = pthread_yield;
	exit(0);
}
