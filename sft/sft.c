/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2006-2015, Pittsburgh Supercomputing Center
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
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <gcrypt.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/crc.h"
#include "pfl/fcntl.h"
#include "pfl/fmt.h"
#include "pfl/listcache.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/opstats.h"
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
	int			 flags;		/* see FF_* below */
	int			 refcnt;
	int			 rc;		/* file return code */
	struct cksum		 cksum;
	int64_t			 chunkid;
};

#define FF_ZEROES		(1 << 0)	/* data is all zeroes */
#define FF_DONE			(1 << 1)	/* all work enqueued */

/* where to read or write a file */
struct wk {
	struct psc_listentry	 lentry;
	struct file		*f;
	int64_t			 chunkid;
	int64_t			 off;
	ssize_t			 len;
};

int				 sft_alg = CKSUMT_CRC;
int				 docrc;
int				 dowrite;
/*
 * If given, use it instead of the sizes of each individual files.
 * Note, read beyond EOF returns all zero.
 */
int64_t				 totalsz;
int				 chunk;
int				 verbose;
int				 checkzero;
int				 continue_after_errors;
int				 piecewise;
volatile sig_atomic_t		 exit_from_signal;
int				 direct_io;
int				 nthr = 1;
ssize_t				 bufsz = 32 * 1024;
off_t				 seekoff;
struct psc_dynarray		 files;
struct psc_listcache		 wkq;
struct psc_poolmaster		 wk_poolmaster;
struct psc_poolmgr		*wk_pool;
struct pfl_opstat		*iostats;
struct psc_waitq		 display_wq = PSC_WAITQ_INIT("display");
volatile int			 running = 1;

int				 incomplete = 0;

/* only main thread updates it, so it is thread-safe */
long long			 totalbytes;

int 				 seed;

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
	if (--f->refcnt || !(f->flags & FF_DONE)) {
		freelock(&f->lock);
		return (0);
	}

	if (docrc && !chunk && !f->rc) {
		cksum_fini(&f->cksum);

		lock_output();
		printf("F '%s' %c %s\n", f->fn,
		    f->flags & FF_ZEROES ? 'Z' : ' ', f->cksum.buf);
		unlock_output();
	}

	if (close(f->fd) == -1) {
		int save_errno = errno;

		lock_output();
		warnx("close: %s: %s", f->fn, strerror(save_errno));
		unlock_output();
		if (!continue_after_errors)
			exit(1);
	}
	free(f->fn);
	PSCFREE(f);
	return (1);
}

void
thrmain(struct psc_thread *thr)
{
	int i, save_errno;
	struct cksum cksum;
	struct file *f;
	struct wk *wk;
	ssize_t rc;
	char *buf;
	int32_t result;
	char rand_statebuf[32];
	struct random_data rand_state;

	/* use the same buffer for all reads or writes */
	buf = psc_alloc(bufsz, PAF_PAGEALIGN);

	if (dowrite) {
		if (seed) {
			memset(rand_statebuf, 0, sizeof(rand_statebuf));
			memset(&rand_state, 0, sizeof(rand_state));
			initstate_r(seed, rand_statebuf, 
			    sizeof(rand_statebuf), &rand_state);
			for (i = 0; i < bufsz; i++) {
				random_r(&rand_state, &result);
				buf[i] = (unsigned char)result;
			}
		} else
			pfl_random_getbytes(buf, bufsz);
	}

	while (pscthr_run(thr)) {
		if (exit_from_signal)
			break;

		wk = lc_getwait(&wkq);
		if (wk == NULL)
			break;
		f = wk->f;

 nextchunk:
		if (dowrite)
			rc = pwrite(f->fd, buf, wk->len, wk->off);
		else
			rc = pread(f->fd, buf, wk->len, wk->off);
		save_errno = errno;

		if (rc == -1) {
			lock_output();
			warnx("%s: %s: %s", dowrite ? "write" : "read",
			    f->fn, strerror(save_errno));
			unlock_output();
			if (!continue_after_errors)
				exit(1);
			f->rc = -1;
			incomplete = 1;
			goto end;
		}

		if (rc && rc != wk->len) {
			lock_output();
			warnx("%s: %s: unexpected short I/O "
			    "(expected %zd, got %zd)",
			    dowrite ? "write" : "read", f->fn,
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
				if (checkzero && !pfl_memchk(buf, 0,
				    rc)) {
					spinlock(&f->lock);
					f->flags &= ~FF_ZEROES;
					freelock(&f->lock);
				}
				cksum_add(&f->cksum, buf, rc);
				wk->off += wk->len;
				if (wk->off < f->stb.st_size) {
					wk->chunkid++;
					if (wk->off + wk->len >
					    f->stb.st_size)
						wk->len = f->stb.st_size %
						    bufsz;
					if (!exit_from_signal)
						goto nextchunk;
				}
			}
		}

 end:
		file_close(f);
		psc_pool_return(wk_pool, wk);
	}
	psc_free(buf, PAF_PAGEALIGN);
}

void
print_status_line(FILE *fp, uint64_t intv, uint64_t lifetime)
{
	char buf[PSCFMT_HUMAN_BUFSIZ];

	pfl_fmt_human(buf, intv);
	fprintf(fp, "%7s ", buf);
	pfl_fmt_human(buf, lifetime);
	fprintf(fp, "%7s", buf);
}

void
display(__unusedx struct psc_thread *thr)
{
	struct timespec ts, ts0, delta;
	int n, t, istty;
	FILE *fp;

	fp = stdout;

	istty = isatty(STDOUT_FILENO);
	if (istty) {
		n = fprintf(fp, "%7s %7s", "rate", "total");
		fprintf(fp, "\n");
		for (t = 0; t < n; t++)
			fputc('=', fp);
		fprintf(fp, "\naccumulating...");
		fflush(fp);
	}

	PFL_GETTIMESPEC(&ts0);
	ts = ts0;

	while (running) {
		ts.tv_sec += 1;
		psc_waitq_waitabs(&display_wq, NULL, &ts);

		if (istty)
			fprintf(fp, "\r");
		print_status_line(fp,
		    iostats->opst_intv,
		    psc_atomic64_read(&iostats->opst_lifetime));
		if (istty)
			fflush(fp);
		else
			fprintf(fp, "\n");
	}
	timespecsub(&ts, &ts0, &delta);
	if (istty)
		fprintf(fp, "\r");
	print_status_line(fp,
	    psc_atomic64_read(&iostats->opst_lifetime) /
	      (delta.tv_sec + delta.tv_nsec / 1e9),
	    psc_atomic64_read(&iostats->opst_lifetime));
	fprintf(fp, "\n");
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

int
addwk(struct file *f)
{
	struct wk *wk;
	int64_t size;
	int done;

	spinlock(&f->lock);
	f->refcnt++;
	freelock(&f->lock);

	wk = psc_pool_get(wk_pool);
	wk->f = f;
	wk->off = f->chunkid * bufsz;

	size = totalsz ? totalsz : f->stb.st_size;
	if (wk->off + bufsz > size) {
		wk->len = size % bufsz;
	} else
		wk->len = bufsz;
	wk->chunkid = f->chunkid++;
	done = wk->off + bufsz >= size || (docrc && !chunk);

	_lc_add(&wkq, wk, PLCBF_TAIL, NULL);

	if (done) {
		spinlock(&f->lock);
		f->flags |= FF_DONE;
		freelock(&f->lock);

		file_close(f);
		psc_dynarray_remove(&files, f);
	}
	totalbytes += wk->len;
	return (done);
}

int
sft_proc(FTSENT *fe, __unusedx void *arg)
{
	struct file *f;

	if (exit_from_signal) {
		pfl_fts_set(fe, FTS_SKIP);
		return (0);
	}

	if (fe->fts_info != FTS_F &&
	    (fe->fts_statp->st_mode & S_IFBLK) == 0)
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
	f->flags |= FF_ZEROES;
	f->fn = strdup(fe->fts_path);
	if (dowrite)
		f->fd = open(fe->fts_path, O_CREAT | O_RDWR | O_TRUNC |
		    (direct_io ? O_DIRECT : 0), 0600);
	else
		f->fd = open(fe->fts_path, O_RDONLY |
		    (direct_io ? O_DIRECT : 0));
	f->refcnt = 1;
	if (f->fd == -1)
		err(1, "open %s", fe->fts_path);
	f->stb = *fe->fts_statp;

	f->chunkid = seekoff / bufsz;
	psc_dynarray_add(&files, f);

	return (0);
}

__dead void
usage(void)
{
	extern const char *__progname;

	fprintf(stderr,
	    "usage: %s [-BcdKPRvwZ] [-b bufsz] [-O offset] [-s size] [-t nthr] file ...\n",
	    __progname);
	exit(1);
}

void
handle_signal(__unusedx int sig)
{
	exit_from_signal = 1;
	incomplete = 1;
}

int
main(int argc, char *argv[])
{
	char hbuf[PSCFMT_HUMAN_BUFSIZ];
	int totalwk = 0, displaybw = 0, c, n, flags = PFL_FILEWALKF_NOCHDIR;
	struct psc_thread **thrv, *dispthr;
	struct timeval t1, t2, t3;
	struct sigaction sa;
	struct file *f;
	double rate;
	char *endp;

	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	if (!gcry_check_version(GCRYPT_VERSION))
		errx(1, "libgcrypt version mismatch");
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);

	pfl_init();
	while ((c = getopt(argc, argv, "Bb:CcD:dKO:PRs:S:Tt:vwZ")) != -1)
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
		case 'C': /* continue after IO errors */
			continue_after_errors = 1;
			break;
		case 'c': /* perform checksums */
			docrc = 1;
			break;
		case 'D': /* select digest */
			choosedigest(optarg);
			docrc = 1;
			break;
		case 'd': /* direct I/O (O_DIRECT) */
			direct_io = 1;
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
		case 'P': /* process files piecewise */
			piecewise = 1;
			break;
		case 'R': /* recursive */
			flags |= PFL_FILEWALKF_RECURSIVE;
			break;
		case 's': /* size */
			totalsz = pfl_humantonum(optarg);
			if (totalsz <= 0)
				errx(1, "%s: %s", optarg, strerror(
				    totalsz ? -totalsz : EINVAL));
			break;
		case 'S': /* seed */
			seed = atoi(optarg);
			break;
		case 'T': /* report total */
			break;
		case 't': /* #threads */
			if (strcmp(optarg, "a") == 0)
				nthr = nprocessors(); // - getloadavg()
			else {
				nthr = strtol(optarg, &endp, 10);
				if (nthr < 1 || nthr > 4096 ||
				    *endp || optarg == endp)
					errx(1, "%s: invalid", optarg);
			}
			break;
		case 'v': /* verbose */
			verbose = 1;
			break;
		case 'w': /* dowrite */
			dowrite = 1;
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

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		psc_fatal("sigaction");

	lc_init(&wkq, "work", struct wk, lentry);
	psc_poolmaster_init(&wk_poolmaster, struct wk, lentry,
	    PPMF_AUTO, nthr, nthr, 0,  NULL, "wk");
	wk_pool = psc_poolmaster_getmgr(&wk_poolmaster);

	pscthrs_init();
	pscthr_init(PFL_THRT_CTL, NULL, 0, "sftctl");
	pfl_opstimerthr_spawn(THRT_OPSTIMER, "opstimerthr");
	iostats = pfl_opstat_init("iostats");

	thrv = PSCALLOC(sizeof(*thrv) * nthr);

	for (n = 0; n < nthr; n++)
		thrv[n] = pscthr_init(0, thrmain, 0, "thr%d", n);

	for (; *argv; argv++)
		pfl_filewalk(*argv, flags, NULL, sft_proc, NULL);

	if (displaybw)
		dispthr = pscthr_init(0, display, 0, "disp");

	gettimeofday(&t1, NULL);

	if (piecewise) {
		while (psc_dynarray_len(&files))
			DYNARRAY_FOREACH(f, n, &files) {
				if (exit_from_signal)
					goto done;
				addwk(f);
				totalwk++;
			}
	} else {
		DYNARRAY_FOREACH(f, n, &files)
			for (;;) {
				totalwk++;
				if (addwk(f))
					break;
				if (exit_from_signal)
					goto done;
			}
	}

 done:
	lc_kill(&wkq);
	for (n = 0; n < nthr; n++)
		pthread_join(thrv[n]->pscthr_pthread, NULL);

	gettimeofday(&t2, NULL);

	if (displaybw) {
		running = 0;
		psc_waitq_wakeall(&display_wq);
		pthread_join(dispthr->pscthr_pthread, NULL);
	}
	if (!verbose || incomplete)
		return (0);

	printf("\n");
	printf("I/O buffer size = %zd.\n", bufsz);
	printf("Total number of threads = %d.\n", nthr);
	printf("Total number of work items added to the list: %d\n", totalwk);

	/*
	 * The following is an alternative way to calculate bandwidth.
	 * Note that totalbytes is only valid when all I/Os are complete
	 * normally.
	 *
	 * If the read or write performance takes a long time to ramp up,
	 * the following numbers will be different from last-second rate.
	 */
	if (t2.tv_usec < t1.tv_usec) {
		t2.tv_usec += 1000000;
		t2.tv_sec--;
	}

	t3.tv_sec = t2.tv_sec - t1.tv_sec;
	t3.tv_usec = t2.tv_usec - t1.tv_usec;

	pfl_fmt_human(hbuf, totalbytes);
	printf("\nTotal time: %"PSCPRI_TIMET" seconds, "
	    "%"PSCPRI_UTIMET" useconds, %s\n",
	    t3.tv_sec, t3.tv_usec, hbuf);

	rate = totalbytes / ((t3.tv_sec * 1000000 + t3.tv_usec) * 1e-6);
	pfl_fmt_human(hbuf, rate);
	printf("%s rate is %s/s\n", dowrite ? "Write" : "Read", hbuf);

	return (0);
}
