/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/pfl.h"
#include "psc_util/journal.h"
#include "psc_util/log.h"

#include "mkfn.h"
#include "pathnames.h"
#include "slerr.h"
#include "sljournal.h"

int format;
int query;
int verbose;
const char *datadir = SL_PATH_DATADIR;
const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-fqsv] [-D dir] [-b dev] [-n entries]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char c, fn[PATH_MAX];
	int rc, fname=0;
	unsigned int options;
	ssize_t nents=SLJ_MDS_JNENTS;

	pfl_init();
	options = PJH_OPT_NONE;
	progname = argv[0];
	while ((c = getopt(argc, argv, "D:n:b:fqv")) != -1)
		switch (c) {
		case 'D':
			datadir = optarg;
			break;
		case 'b':
			fname = 1;
			strncpy(fn, optarg, PATH_MAX);
			break;
		case 'f':
			format = 1;
			break;
		case 'q':
			query = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'n':
			nents = strtol(optarg, NULL, 10);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc)
		usage();

	if (!format && !query)
		usage();

	if (!fname) {
		if (mkdir(datadir, 0700) == -1)
			if (errno != EEXIST)
				err(1, "mkdir: %s", datadir);
		
		xmkfn(fn, "%s/%s", datadir, SL_FN_OPJOURNAL);
	}

	if (format) {
		rc = pjournal_format(fn, nents, SLJ_MDS_ENTSIZE, SLJ_MDS_RA);
		if (rc)
			psc_fatalx("failing formatting journal %s: %s",
			    fn, slstrerror(rc));
		if (verbose)
			warnx("created log file %s with %d %d-byte entries",
			    fn, SLJ_MDS_JNENTS, SLJ_MDS_ENTSIZE);
	}
	if (query)
		pjournal_dump(fn, verbose);
	exit(0);
}
