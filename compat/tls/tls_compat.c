/* $Id$ */

#include <pthread.h>
#include <stdlib.h>

__thread int t;

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	t = 5;
	exit(0);
}
