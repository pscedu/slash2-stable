/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/crc.h"
#include "pfl/fmt.h"
#include "pfl/iostats.h"
#include "pfl/listcache.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/pool.h"
#include "pfl/str.h"
#include "pfl/thread.h"
#include "pfl/timerthr.h"
#include "pfl/types.h"
#include "pfl/walk.h"

#define THRT_TIOS		0	/* timed I/O stats */

struct wk {
	struct psc_listentry lentry;
	struct f	*f;
	int64_t		 chunkid;
	size_t		 off;
};

struct f {
	char		*fn;
	struct stat	 stb;
	int		 fd;
	int		 done;
	int64_t		 nchunks_total;
	psc_atomic64_t	 nchunks_proc;
};

#define NTHR_AUTO (-1)

int			 docrc;
int			 doread = 1;
int			 chunk;
int			 checkzero;
int			 nthr = 1;
ssize_t			 bufsz = 32 * 1024;
uint64_t		 filecrc;
off_t			 seekoff;

struct psc_poolmaster	 wk_poolmaster;
struct psc_poolmgr	*wk_pool;
struct psc_iostats	 ist;

const char		*progname;

struct psc_listcache	 wkq;

void
thrmain(struct psc_thread *thr)
{
	ssize_t rc, bsz;
	struct wk *wk;
	uint64_t crc;
	char *buf;
	int eof;

	buf = PSCALLOC(bufsz);

	while (pscthr_run(thr)) {
		wk = lc_getwait(&wkq);
		if (wk == NULL)
			break;

		if (doread)
			rc = pread(wk->f->fd, buf, bufsz, wk->off);
		else
			rc = pwrite(wk->f->fd, buf, bufsz, wk->off);

		eof = 0;
		if (psc_atomic64_inc_getnew(&wk->f->nchunks_proc) ==
		    wk->f->nchunks_total)
			eof = 1;

		bsz = bufsz;
		if (wk->f->done &&
		    wk->f->nchunks_total - 1 == wk->chunkid)
			bsz = wk->f->stb.st_size % bufsz;
		if (bsz == 0)
			bsz = bufsz;

		if (rc > 0 && rc != bsz) {
			rc = -1;
			errno = EIO;
		}
		if (rc == -1)
			err(1, "%s", wk->f->fn);

		psc_iostats_intv_add(&ist, rc);

		if (docrc) {
			if (chunk) {
				psc_crc64_calc(&crc, buf, rc);

				flockfile(stdout);
				fprintf(stdout, "F '%s' %5zd %c "
				    "CRC=%"PSCPRIxCRC64"\n",
				    wk->f->fn, wk->chunkid,
				    checkzero && pfl_memchk(buf, 0, rc) ?
				    'Z' : ' ', crc);
				funlockfile(stdout);
			} else {
				psc_crc64_add(&filecrc, buf, rc);
				if (eof) {
					psc_crc64_fini(&filecrc);
					fprintf(stdout,
					    "F '%s' CRC=%"PSCPRIxCRC64"\n",
					    wk->f->fn, filecrc);
				}
			}
		}

		if (eof) {
			close(wk->f->fd);
			free(wk->f->fn);
			PSCFREE(wk->f);
		}

		psc_pool_return(wk_pool, wk);
	}
}

void
display(__unusedx struct psc_thread *thr)
{
	char ratebuf[PSCFMT_HUMAN_BUFSIZ];
	struct psc_iostats myist;
	int n, t;

	n = printf("%8s %7s", "time (s)", "rate");
	printf("\n");
	for (t = 0; t < n; t++)
		putchar('=');
	printf("\n");

	n = 0;
	for (;;) {
		sleep(1);
		memcpy(&myist, &ist, sizeof(myist));
		psc_fmt_human(ratebuf,
		    psc_iostats_getintvrate(&myist, 0));
		t = printf("\r%7.3fs %7s",
		    psc_iostats_getintvdur(&myist, 0),
		    ratebuf);
		n = MAX(n - t, 0);
		printf("%*.*s ", n, n, "");
		n = t;
		fflush(stdout);
	}
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-BcKRvwZ] [-b bufsz] [-t nthr] [-O offset] file ...\n",
	    progname);
	exit(1);
}

int
nprocessors(void)
{
	int n = 0;
#ifdef HAVE_SCHED_GETAFFINITY
	cpu_set_t *mask = NULL;
	int np = 1024, rc, i;
	size_t size;

	for (;;) {
		mask = CPU_ALLOC(np);
		size = CPU_ALLOC_SIZE(np);
		CPU_ZERO_S(size, mask);
		rc = sched_getaffinity(0, size, mask);
		if (rc == 0)
			break;
		if (rc != EINVAL)
			err(1, "sched_getaffinity");

		rc = errno;
		CPU_FREE(mask);
		np = np << 2;
	}
	for (i = 0; i < np; i++)
		if (CPU_ISSET_S(i, size, mask))
			n++;
	CPU_FREE(mask);
#endif
	return (n);
}

void
addwk(struct f *f, off_t off, int chunkid)
{
	struct wk *wk;

	if (off + bufsz >= f->stb.st_size) {
		f->nchunks_total = chunkid + 1;
		f->done = 1;
	}

	wk = psc_pool_get(wk_pool);
	wk->f = f;
	wk->off = off;
	wk->chunkid = chunkid;
	lc_add(&wkq, wk);
}

int
proc(const char *fn,
    __unusedx const struct stat *stb, int info,
    __unusedx int level, __unusedx void *arg)
{
	int chunkid;
	struct f *f;
	off_t off;

	if (info != PFWT_F)
		return (0);

	f = PSCALLOC(sizeof(*f));
	f->fn = strdup(fn);
	f->fd = open(fn, O_RDONLY);
	if (f->fd == -1)
		err(1, "open %s", fn);
	if (fstat(f->fd, &f->stb) == -1)
		err(1, "stat %s", fn);

	for (chunkid = 0, off = seekoff;
	    off < f->stb.st_size; off += bufsz)
		addwk(f, off, chunkid++);

	return (0);
}

int
main(int argc, char *argv[])
{
	int displaybw = 0, c, n, flags = 0;
	struct psc_thread **thrv;
	char *endp;

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "Bb:cKO:RTt:vwZ")) != -1)
		switch (c) {
		case 'B': /* display bandwidth */
			displaybw = 1;
			break;
		case 'b': /* I/O block size */
			bufsz = pfl_humantonum(optarg);
			if (bufsz <= 0)
				errx(1, "%s: %s", optarg, strerror(
				    bufsz ? -bufsz : EINVAL));
			break;
		case 'c': /* perform CRC of entire file */
			docrc = 1;
			psc_crc64_init(&filecrc);
			break;
		case 'K': /* report checksum of each file chunk */
			chunk = 1;
			break;
		case 'O': /* offset */
			seekoff = pfl_humantonum(optarg);
			if (seekoff < 0)
				errx(1, "%s: %s", optarg,
				    strerror(-seekoff));
			break;
		case 'R': /* recursive */
			flags |= PFL_FILEWALKF_RECURSIVE;
			break;
		case 'T': /* report total */
			break;
		case 't': /* #threads */
			if (strcmp(optarg, "a") == 0)
				nthr = nprocessors();
			else {
				nthr = strtol(optarg, &endp, 10);
				/* XXX check */
			}
			break;
		case 'v': /* verbose */
			flags |= PFL_FILEWALKF_VERBOSE;
			break;
		case 'w': /* write */
			doread = 0;
			break;
		case 'Z': /* report if file chunk is all zeroes */
			checkzero = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();

	if (nthr && docrc && !chunk)
		errx(1, "cannot parallelize filewide CRC");

	if (displaybw)
		pscthr_init(0, display, NULL, 0, "disp");

	lc_init(&wkq, struct wk, lentry);
	psc_poolmaster_init(&wk_poolmaster, struct wk, lentry,
	    0, nthr, nthr, 0, NULL, NULL, NULL, "wk");
	wk_pool = psc_poolmaster_getmgr(&wk_poolmaster);

	psc_tiosthr_spawn(THRT_TIOS, "tiosthr");
	psc_iostats_init(&ist, "ist");

	thrv = PSCALLOC(sizeof(*thrv) * nthr);

	for (n = 0; n < nthr; n++)
		thrv[n] = pscthr_init(0, thrmain, NULL, 0, "thr%d", n);

	for (; *argv; argv++)
		pfl_filewalk(*argv, flags, NULL, proc, NULL);

	lc_kill(&wkq);
	for (n = 0; n < nthr; n++)
		pthread_join(thrv[n]->pscthr_pthread, NULL);

	if (displaybw)
		printf("\n");

	return (0);
}
