/* $Id$ */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_util/acsvc.h"
#include "psc_util/cdefs.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int fd;

	progname = argv[0];
	while (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	acsvc_init(0, "test", argv);

	fd = access_fsop(ACSOP_OPEN, geteuid(), getegid(),
	    __FILE__, O_RDONLY);
	if (fd == -1)
		psc_fatal("access_fsop");
	close(fd);
	exit(0);
}
