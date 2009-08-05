/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/cdefs.h"
#include "psc_util/setprocesstitle.h"

int
main(__unusedx int argc, char *argv[])
{
	setprocesstitle(argv, "foobar %d", 13);
	sleep(10);
	exit(0);
}
