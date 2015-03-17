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
#include <gcrypt.h>
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

GCRY_THREAD_OPTION_PTHREAD_IMPL;

#define THRT_OPSTIMER		0	/* timed opstats */

#define NTHR_AUTO		(-1)

#define CKSUM_BUFSZ		64

#define CKSUMT_CRC		0
#define CKSUMT_MD5		1

const char *sft_algs[] = {
	"crc",
	"md5",
};

struct cksum {
	uint64_t		 crc;
	gcry_md_hd_t		 md;
	char			 buf[CKSUM_BUFSZ];
	int			 gcry_alg;
};

struct file {
	psc_spinlock_t		 lock;
	char			*fn;
	struct stat		 stb;
	int			 fd;
	int			 done;
	int			 refcnt;
	int			 rc;	/* file return code */
	struct cksum		 cksum;
};

struct wk {
	struct psc_listentry	 lentry;
	struct file		*f;
	int64_t			 chunkid;
	int64_t			 off;
	ssize_t			 len;
};

int			 sft_alg = CKSUMT_CRC;
int			 docrc;
int64_t			 writesz;
int			 chunk;
int			 verbose;
int			 checkzero;
int			 nthr = 1;
ssize_t			 bufsz = 32 * 1024;
off_t			 seekoff;

struct psc_poolmaster	 wk_poolmaster;
struct psc_poolmgr	*wk_pool;
struct pfl_opstat	*iostats;
volatile int		 running = 1;

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

void
choosedigest(const char *alg)
{
	for (sft_alg = 0; sft_alg < nitems(sft_algs); sft_alg++)
		if (strcmp(sft_algs[sft_alg], alg) == 0)
			return;
	psc_fatalx("unsupported digest: %s", alg);
}

void
cksum_init(struct cksum *c)
{
	gcry_error_t error;

	switch (sft_alg) {
	case CKSUMT_CRC:
		psc_crc64_init(&c->crc);
		break;
	case CKSUMT_MD5:
		c->gcry_alg = GCRY_MD_MD5;
		error = gcry_md_open(&c->md, c->gcry_alg, 0);
		if (error)
			errx(1, "%s", gpg_strerror(error));
		break;
	}
}

void
cksum_add(struct cksum *c, void *buf, size_t len)
{
	switch (sft_alg) {
	case CKSUMT_CRC:
		psc_crc64_add(&c->crc, buf, len);
		break;
	case CKSUMT_MD5:
		gcry_md_write(c->md, buf, len);
		break;
	}
}

void
cksum_fini(struct cksum *c)
{
	unsigned char *p;
	int i, len;

	switch (sft_alg) {
	case CKSUMT_CRC:
		psc_crc64_fini(&c->crc);
		snprintf(c->buf, sizeof(c->buf), "%"PSCPRIxCRC64,
		    c->crc);
		break;
	case CKSUMT_MD5:
		gcry_md_final(c->md);
		len = gcry_md_get_algo_dlen(c->gcry_alg);
		p = gcry_md_read(c->md, c->gcry_alg);
		for (i = 0; i < len; i++) {
			snprintf(c->buf + i*2, sizeof(c->buf) - i*2,
			    "%02x,", p[i]);
		}
		c->buf[i*2] = '\0';
		gcry_md_close(c->md);
		break;
	}
}

int
file_close(struct file *f)
{
	spinlock(&f->lock);
	if (--f->refcnt || !f->done) {
		freelock(&f->lock);
		return (0);
	}

	if (docrc && !chunk && !f->rc) {
		cksum_fini(&f->cksum);

		lock_output();
		printf("F '%s' %s\n", f->fn, f->cksum.buf);
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
	struct cksum cksum;
	struct file *f;
	struct wk *wk;
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

 nextchunk:
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

		pfl_opstat_add(iostats, rc);

		if (docrc) {
			if (chunk) {
				cksum_init(&cksum);
				cksum_add(&cksum, buf, rc);
				cksum_fini(&cksum);

				lock_output();
				printf("F '%s' %5zd %c %s\n",
				    f->fn, wk->chunkid,
				    checkzero && pfl_memchk(buf, 0, rc) ?
				    'Z' : ' ', cksum.buf);
				unlock_output();
			} else {
				cksum_add(&f->cksum, buf, rc);
				wk->off += wk->len;
				if (wk->off < f->stb.st_size) {
					wk->chunkid++;
					if (wk->off + wk->len >
					    f->stb.st_size)
						wk->len = f->stb.st_size %
						    bufsz;
					goto nextchunk;
				}
			}
		}

 end:
		file_close(f);
		psc_pool_return(wk_pool, wk);
	}
}

struct psc_waitq display_wq = PSC_WAITQ_INIT;

void
display(__unusedx struct psc_thread *thr)
{
	struct timespec ts;
	char ratebuf[PSCFMT_HUMAN_BUFSIZ];
	FILE *fp;
	int n, t;

	fp = stdout;

	n = fprintf(fp, "%7s", "rate");
	fprintf(fp, "\n");
	for (t = 0; t < n; t++)
		fputc('=', fp);
	fprintf(fp, "\naccum...");
	fflush(fp);

	PFL_GETTIMESPEC(&ts);

	n = 0;
	while (running) {
		ts.tv_sec += 1;
		psc_waitq_waitabs(&display_wq, NULL, &ts);

		psc_fmt_human(ratebuf, iostats->opst_last);
		t = fprintf(fp, "\r%7s", ratebuf);
		n = MAX(n - t, 0);
		fprintf(fp, "%*.*s ", n, n, "");
		n = t;
		fflush(fp);
	}
	printf("\n");
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
proc(FTSENT *fe, __unusedx void *arg)
{
	struct file *f;
	int chunkid;
	off_t off;

	if (fe->fts_info != FTS_F)
		return (0);

	if (verbose) {
		lock_output();
		warnx("processing %s", fe->fts_path);
		unlock_output();
	}

	f = PSCALLOC(sizeof(*f));
	if (docrc && !chunk)
		cksum_init(&f->cksum);
	INIT_SPINLOCK(&f->lock);
	f->fn = strdup(fe->fts_path);
	if (writesz)
		f->fd = open(fe->fts_path, O_CREAT | O_RDWR, 0600);
	else
		f->fd = open(fe->fts_path, O_RDONLY);
	f->refcnt = 1;
	if (f->fd == -1)
		err(1, "open %s", fe->fts_path);
	f->stb = *fe->fts_statp;

	if (writesz)
		f->stb.st_size = writesz;

	for (chunkid = 0, off = seekoff;
	    off < f->stb.st_size; off += bufsz) {
		addwk(f, off, chunkid++,
		    off + bufsz > f->stb.st_size ?
		    f->stb.st_size % bufsz : bufsz);
		if (docrc && !chunk)
			break;
	}

	spinlock(&f->lock);
	f->done = 1;
	freelock(&f->lock);

	file_close(f);

	return (0);
}

int
main(int argc, char *argv[])
{
	int displaybw = 0, c, n, flags = PFL_FILEWALKF_NOCHDIR;
	struct psc_thread **thrv, *dispthr;
	char *endp;

	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	if (!gcry_check_version(GCRYPT_VERSION))
		errx(1, "libgcrypt version mismatch");
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

	pfl_init();
	progname = argv[0];
	while ((c = getopt(argc, argv, "Bb:cD:KO:RTt:vw:Z")) != -1)
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
		case 'c': /* perform checksums */
			docrc = 1;
			break;
		case 'D': /* select digest */
			choosedigest(optarg);
			docrc = 1;
			break;
		case 'K': /* report checksum of each file chunk */
			chunk = 1;
			docrc = 1;
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
			writesz = pfl_humantonum(optarg);
			if (writesz <= 0)
				errx(1, "%s: %s", optarg, strerror(
				    writesz ? -writesz : EINVAL));
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

	if (displaybw)
		dispthr = pscthr_init(0, display, NULL, 0, "disp");

	lc_init(&wkq, struct wk, lentry);
	psc_poolmaster_init(&wk_poolmaster, struct wk, lentry,
	    0, nthr, nthr, 0, NULL, NULL, NULL, "wk");
	wk_pool = psc_poolmaster_getmgr(&wk_poolmaster);

	pfl_opstimerthr_spawn(THRT_OPSTIMER, "opstimerthr");
	iostats = pfl_opstat_init("iostats");

	thrv = PSCALLOC(sizeof(*thrv) * nthr);

	for (n = 0; n < nthr; n++)
		thrv[n] = pscthr_init(0, thrmain, NULL, 0, "thr%d", n);

	for (; *argv; argv++)
		pfl_filewalk(*argv, flags, NULL, proc, NULL);

	lc_kill(&wkq);
	for (n = 0; n < nthr; n++)
		pthread_join(thrv[n]->pscthr_pthread, NULL);

	if (displaybw) {
		running = 0;
		psc_waitq_wakeall(&display_wq);
		pthread_join(dispthr->pscthr_pthread, NULL);
	}

	return (0);
}
