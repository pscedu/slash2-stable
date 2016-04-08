/* $Id$ */

#include <execinfo.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	void *buffer[32];

	(void)argc;
	(void)argv;
	backtrace(buffer, 32);
	exit(0);
}
