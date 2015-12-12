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

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>

void
pr(const char *name, uint64_t value)
{
	static int i;
	int n;

	if (i++ % 2) {
		n = printf("%s ", name);
		while (n++ <= 50)
			putchar('-');
		if (n < 53)
			printf("> ");
		printf("%"PRIu64"\n", value);
	} else
		printf("%-52s %"PRIu64"\n", name, value);
}

#define PRTYPE(type)	pr(#type, sizeof(type))
#define PRVAL(val)	pr(#val, (unsigned long)(val))

#include "typedump.h"

#include "lnet/socklnd.h"

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "")) != -1)
		switch (c) {
		default:
			usage();
		}
	argc -= optind;
	if (argc)
		usage();

	/* base types/values */
	PRTYPE(int);
	PRTYPE(unsigned char);
	PRTYPE(unsigned short);
	PRTYPE(void *);
	PRTYPE(uint8_t);
	PRTYPE(uint16_t);
	PRTYPE(uint32_t);
	PRTYPE(uint64_t);
	PRTYPE(int8_t);
	PRTYPE(int16_t);
	PRTYPE(int32_t);
	PRTYPE(int64_t);
	PRTYPE(INT_MAX);
	PRTYPE(UINT64_MAX);

	/* system types/values */
	PRVAL(PATH_MAX);

	PRVAL(sizeof(((struct stat *)NULL)->st_dev));
	PRVAL(sizeof(((struct stat *)NULL)->st_nlink));
	PRVAL(sizeof(((struct stat *)NULL)->st_blksize));
	PRVAL(sizeof(((struct stat *)NULL)->st_size));
	PRVAL(sizeof(((struct stat *)NULL)->st_mtime));
#ifdef HAVE_STB_MTIM
	PRVAL(sizeof(((struct stat *)NULL)->st_mtim));
#endif

	PRTYPE(dev_t);
	PRTYPE(ino_t);
	PRTYPE(mode_t);
	PRTYPE(nlink_t);
	PRTYPE(uid_t);
	PRTYPE(gid_t);
	PRTYPE(off_t);
	PRTYPE(typeof(((struct stat *)NULL)->st_blksize));
	PRTYPE(typeof(((struct stat *)NULL)->st_blocks));
	PRTYPE(time_t);

	PRTYPE(rlim_t);

	PRTYPE(lnet_nid_t);
	PRTYPE(lnet_pid_t);
	PRTYPE(lnet_process_id_t);
	PRTYPE(ksock_msg_t);

	typedump();

	PRVAL(offsetof(struct psc_listcache, plc_listhd));
	PRVAL(offsetof(struct psc_journal_enthdr, pje_data));
	PRVAL(PSCFMT_HUMAN_BUFSIZ);
	PRVAL(PSCFMT_RATIO_BUFSIZ);
	PRVAL(PSCRPC_NIDSTR_SIZE);

	exit(0);
}
