/* $Id$ */

#include <signal.h>
#include <stdlib.h>

extern const char * const sys_sigabbrev[];

int
main(int argc, char *argv[])
{
	const void *p;

	(void)argc;
	(void)argv;
	p = sys_sigabbrev;
	exit(0);
}
