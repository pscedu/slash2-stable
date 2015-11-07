/* $Id$ */

#include <stdlib.h>

__thread int t;

int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
#ifndef __LP64__
#error 64-bit support unavailable
#endif
	exit(0);
}
