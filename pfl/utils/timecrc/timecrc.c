/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/crc.h"
#include "pfl/log.h"
#include "pfl/time.h"
#include "pfl/types.h"

const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s file\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct timeval tm0, tm1, tmd, tm_total, tm_last;
	struct stat stb;
	char buf[BUFSIZ];
	uint64_t crc;
	const char *fn;
	int fd, pad;
	size_t acsz;
	ssize_t rc;

	progname = argv[0];
	if (getopt(argc, argv, "") != -1)
		usage();
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();
	fn = argv[0];

	fd = open(fn, O_RDONLY);
	if (fd == -1)
		psc_fatal("%s", fn);
	if (fstat(fd, &stb) == -1)
		psc_fatal("fstat %s", fn);

	pad = snprintf(NULL, 0, "%"PSCPRIdOFFT, stb.st_size);

	acsz = 0;
	psc_crc64_init(&crc);
	memset(&tm_total, 0, sizeof(tm_total));
	memset(&tm_last, 0, sizeof(tm_last));
	for (;;) {
		rc = read(fd, buf, sizeof(buf));
		if (rc == -1)
			psc_fatal("read %s", fn);
		if (rc == 0)
			break;
		acsz += rc;

		PFL_GETTIMEVAL(&tm0);
		psc_crc64_add(&crc, buf, rc);
		PFL_GETTIMEVAL(&tm1);
		timersub(&tm1, &tm0, &tmd);

		memcpy(&tm0, &tm_total, sizeof(tm0));
		timeradd(&tmd, &tm0, &tm_total);

		timersub(&tm1, &tm_last, &tmd);
		if (tmd.tv_usec > 1000000 / 32) { /* 32 fps */
			memcpy(&tm_last, &tm1, sizeof(tm_last));
			printf("\r%*zd/%"PSCPRIdOFFT, pad, acsz,
			    stb.st_size);
			fflush(stdout);
		}
	}
	close(fd);

	psc_crc64_fini(&crc);
	printf("\rcrc %"PSCPRIxCRC64" size %"PSCPRIdOFFT" time %.3fs\n",
	    crc, stb.st_size, tm_total.tv_sec + tm_total.tv_usec * 1e-6);
	exit(0);
}
