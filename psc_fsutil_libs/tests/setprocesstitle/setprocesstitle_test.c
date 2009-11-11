/* $Id$ */

#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "psc_util/log.h"
#include "psc_util/setprocesstitle.h"

#define _PATH_CMDLINE "/proc/self/cmdline"

int
main(__unusedx int argc, char *argv[])
{
	char buf[BUFSIZ];
	int fd;

	setprocesstitle(argv, "foobar %d", 13);

	fd = open(_PATH_CMDLINE, O_RDONLY);
	if (fd == -1)
		err(1, _PATH_CMDLINE);
	read(fd, buf, sizeof(buf));
	close(fd);

	psc_assert(strcmp(buf, "foobar 13") == 0);
	exit(0);
}
