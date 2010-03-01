/* $Id$ */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "psc_util/base64.h"
#include "psc_util/log.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s file ...\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct stat stb;
	char *buf;
	void *p;
	int fd;

	pfl_init();
	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	argv += optind;
	if (!argc)
		usage();

	buf = NULL;
	for (; *argv; argv++) {
		fd = open(*argv, O_RDONLY);
		if (fd == -1) {
			warn("%s", *argv);
			continue;
		}
		if (fstat(fd, &stb) == -1) {
			warn("fstat %s", *argv);
			goto next;
		}

		p = mmap(NULL, stb.st_size, PROT_READ,
		    MAP_SHARED, fd, 0);
		if (p == MAP_FAILED) {
			warn("mmap %s", *argv);
			goto next;
		}

		buf = realloc(buf, 4 * stb.st_size / 3 + 2);
		if (buf == NULL)
			psc_fatal("realloc");
		psc_base64_encode(p, buf, stb.st_size);
		printf("base64(%s)=%s\n", *argv, buf);
		munmap(p, stb.st_size);
 next:
		close(fd);
	}
	exit(0);
}
