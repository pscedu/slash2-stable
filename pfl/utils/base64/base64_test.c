/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

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
#include "pfl/base64.h"
#include "pfl/log.h"

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
		pfl_base64_encode(p, buf, stb.st_size);
		printf("base64(%s)=%s\n", *argv, buf);
		munmap(p, stb.st_size);
 next:
		close(fd);
	}
	exit(0);
}
