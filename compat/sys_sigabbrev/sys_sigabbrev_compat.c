/* $Id$ */

#include <signal.h>
#include <stdlib.h>

extern const char * const sys_sigabbrev[];

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	(void)sys_sigabbrev;
	exit(0);
}
