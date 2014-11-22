/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
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
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/pool.h"
#include "pfl/random.h"
#include "pfl/str.h"
#include "pfl/thread.h"
#include "pfl/timerthr.h"
#include "pfl/types.h"
#include "pfl/walk.h"

#define THRT_TIOS		0	/* timed I/O stats */

#define NTHR_AUTO		(-1)

struct file {
	psc_spinlock_t		 lock;
	char			*fn;
	struct stat		 stb;
	int			 fd;
	int			 done;
	int			 refcnt;
	int			 rc;	/* file return code */
};


struct wk {
	struct psc_listentry	 lentry;
	struct file		*f;
	int64_t			 chunkid;
	size_t			 off;
	ssize_t			 len;
};

int			 docrc;
uint64_t		 writesz;
int			 chunk;
int			 verbose;
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

#define lock_output()							\
	do {								\
		flockfile(stdout);					\
		flockfile(stderr);					\
	} while (0)

#define unlock_output()							\
	do {								\
		funlockfile(stderr);					\
		funlockfile(stdout);					\
	} while (0)

int
file_close(struct file *f)
{
	spinlock(&f->lock);
	if (--f->refcnt || !f->done) {
		freelock(&f->lock);
		return (0);
	}

	if (docrc && !chunk && !f->rc) {
		psc_crc64_fini(&filecrc);

		lock_output();
		printf("F '%s' CRC=%"PSCPRIxCRC64"\n",
		    f->fn, filecrc);
		unlock_output();
	}

	close(f->fd);
	free(f->fn);
	PSCFREE(f);
	return (1);
}

void
thrmain(struct psc_thread *thr)
{
	int save_errno;
	struct file *f;
	struct wk *wk;
	uint64_t crc;
	ssize_t rc;
	char *buf;

	buf = PSCALLOC(bufsz);

	if (writesz)
		pfl_random_getbytes(buf, bufsz);

	while (pscthr_run(thr)) {
		wk = lc_getwait(&wkq);
		if (wk == NULL)
			break;
		f = wk->f;

		if (writesz)
			rc = pwrite(f->fd, buf, wk->len, wk->off);
		else
			rc = pread(f->fd, buf, wk->len, wk->off);
		save_errno = errno;

		if (rc == -1) {
			lock_output();
			warnx("%s: %s: %s", writesz ? "write" : "read",
			    f->fn, strerror(save_errno));
			unlock_output();
			f->rc = -1;
			goto end;
		}

		if (rc && rc != wk->len) {
			lock_output();
			warnx("%s: %s: unexpected short I/O "
			    "(expected %zd, got %zd)",
			    writesz ? "write" : "read", f->fn,
			    wk->len, rc);
			unlock_output();
			f->rc = -1;
		}

		psc_iostats_intv_add(&ist, rc);

		if (docrc) {
			if (chunk) {
				psc_crc64_calc(&crc, buf, rc);

				lock_output();
				printf("F '%s' %5zd %c "
				    "CRC=%"PSCPRIxCRC64"\n",
				    f->fn, wk->chunkid,
				    checkzero && pfl_memchk(buf, 0, rc) ?
				    'Z' : ' ', crc);
				unlock_output();
			} else {
				psc_crc64_add(&filecrc, buf, rc);
			}
		}

 end:
		file_close(f);
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
addwk(struct file *f, off_t off, int chunkid, size_t len)
{
	struct wk *wk;

	spinlock(&f->lock);
	f->refcnt++;
	freelock(&f->lock);

	wk = psc_pool_get(wk_pool);
	wk->f = f;
	wk->len = len;
	wk->off = off;
	wk->chunkid = chunkid;
	lc_add(&wkq, wk);
}

int
proc(const char *fn, const struct stat *stb, int info,
    __unusedx int level, __unusedx void *arg)
{
	struct file *f;
	int chunkid;
	off_t off;

	if (info != PFWT_F)
		return (0);

	if (verbose) {
		lock_output();
		warnx("processing %s", fn);
		unlock_output();
	}

	f = PSCALLOC(sizeof(*f));
	INIT_SPINLOCK(&f->lock);
	f->fn = strdup(fn);
	if (writesz)
		f->fd = open(fn, O_CREAT | O_RDWR, 0600);
	else
		f->fd = open(fn, O_RDONLY);
	f->refcnt = 1;
	if (f->fd == -1)
		err(1, "open %s", fn);
	memcpy(&f->stb, stb, sizeof(f->stb));

	if (writesz)
		f->stb.st_size = writesz;

	for (chunkid = 0, off = seekoff;
	    off < f->stb.st_size; off += bufsz)
		addwk(f, off, chunkid++,
		    off + bufsz > f->stb.st_size ?
		    f->stb.st_size % bufsz : bufsz);

	spinlock(&f->lock);
	f->done = 1;
	freelock(&f->lock);

	file_close(f);

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
	while ((c = getopt(argc, argv, "Bb:cKO:RTt:vw:Z")) != -1)
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
				nthr = nprocessors(); // XXX - getload()
			else {
				nthr = strtol(optarg, &endp, 10);
				/* XXX check */
			}
			break;
		case 'v': /* verbose */
			verbose = 1;
			break;
		case 'w': /* write */
			writesz = strtol(optarg, &endp, 10);
			/* XXX check */
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
