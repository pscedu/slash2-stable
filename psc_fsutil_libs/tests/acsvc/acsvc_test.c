/* $Id$ */

#include <sys/param.h>

#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/pfl.h"
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
	char *dir, fn[PATH_MAX];
	int fd;

	progname = argv[0];
	while (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	if (argc)
		usage();

	pfl_init();
	acsvc_init(0, "test", argv);

	dir = strdup(progname);
	if (dir == NULL)
		psc_fatal("strdup");
	if (dirname(dir) == NULL)
		psc_fatal("dirname");
	snprintf(fn, sizeof(fn), "%s%s%s",
	    __FILE__[0] == '/' ? "" : dir,
	    __FILE__[0] == '/' ? "" : "/", __FILE__);

	fd = access_fsop(ACSOP_OPEN, geteuid(), getegid(), fn,
	    O_RDONLY);
	if (fd == -1)
		psc_fatal("access_fsop: %s", fn);
	close(fd);
	exit(0);
}
