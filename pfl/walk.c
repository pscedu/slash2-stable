/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifdef HAVE_FTS
#  undef _FILE_OFFSET_BITS	/* FTS is not 64-bit ready */
#endif

#include <sys/stat.h>

#include <err.h>

#ifdef HAVE_FTS
#include <fts.h>
#else
#include <ftw.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/str.h"
#include "pfl/walk.h"

int
pfl_filewalk_stm2info(int mode)
{
	if (S_ISREG(mode))
		return (PFWT_F);
	if (S_ISDIR(mode))
		return (PFWT_D);
	psc_fatalx("invalid mode %#o", mode);
}

/*
 * Traverse a file hierarchy with a given operation.
 * @fn: file root.
 * @flags: behavorial flags.
 * @cmpf: optional dirent comparator for ordering.
 * @cbf: callback to invoke on each file.
 * @arg: optional argument to supply to callback.
 * Notes: the callback will be invoked with a fully resolved absolute
 *	path name unless the file in question is a symbolic link.
 */
#ifdef HAVE_FTS

int
pfl_filewalk(const char *fn, int flags, void *cmpf,
    int (*cbf)(const char *, const struct stat *, int, int, void *),
    void *arg)
{
	char * const pathv[] = { (char *)fn, NULL };
	int rc = 0, ptype, f_flags = 0;
	char buf[PATH_MAX];
	const char *path;
	struct stat stb;
	FTSENT *f;
	FTS *fp;

	if (flags & PFL_FILEWALKF_RECURSIVE) {
		if (flags & PFL_FILEWALKF_NOSTAT)
			f_flags |= FTS_NOSTAT;
		fp = fts_open(pathv, f_flags | FTS_COMFOLLOW |
		    FTS_PHYSICAL, cmpf);
		if (fp == NULL)
			psc_fatal("fts_open %s", fn);
		while ((f = fts_read(fp)) != NULL) {
			switch (f->fts_info) {
			case FTS_NS:
				psclog_warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			case FTS_F:
				ptype = PFWT_F;
				if (0)
				/* FALLTHROUGH */
			case FTS_D:
				ptype = PFWT_D;
				if (0)
				/* FALLTHROUGH */
			case FTS_DP:
				ptype = PFWT_DP;
				path = buf;
				if (flags & PFL_FILEWALKF_RELPATH)
					path = f->fts_path;
				else if (realpath(f->fts_path, buf) == NULL) {
					psclog_warn("realpath %s",
					    f->fts_path);
					break;
				}
				if (flags & PFL_FILEWALKF_VERBOSE)
					warnx("processing %s%s",
					    path, f->fts_info == FTS_F ?
					    "" : "/");
				rc = cbf(path, f->fts_statp, ptype,
				    f->fts_level, arg);
				if (rc == PFL_FILEWALK_RC_SKIP)
					fts_set(fp, f, FTS_SKIP);
				else if (rc) {
					fts_close(fp);
					return (rc);
				}
				break;
			case FTS_SL:
				if (flags & PFL_FILEWALKF_VERBOSE)
					warnx("processing %s",
					    f->fts_path);
				rc = cbf(f->fts_path, f->fts_statp,
				    PFWT_SL, f->fts_level, arg);
				if (rc == PFL_FILEWALK_RC_SKIP)
					fts_set(fp, f, FTS_SKIP);
				else if (rc) {
					fts_close(fp);
					return (rc);
				}
				break;
			default:
				psclog_warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			}
		}
		fts_close(fp);
	} else {
		if (lstat(fn, &stb) == -1)
			err(1, "%s", fn);
		if (flags & PFL_FILEWALKF_RELPATH)
			path = fn;
		else {
			if (realpath(fn, buf) == NULL)
				err(1, "%s", fn);
			path = buf;
		}
		ptype = pfl_filewalk_stm2info(stb.st_mode);
		rc = cbf(path, &stb, ptype, 0, arg);
	}
	return (rc);
}

#else

int (*pfl_filewalk_cbf)(const char *, const struct stat *, int, int,
    void *);
void *pfl_filewalk_arg;

int
pfl_filewalk_cb(const char *fn, const struct stat *stb, int wtype,
    struct FTW *ftw)
{
	char buf[PATH_MAX];
	int rc, ptype;

	switch (wtype) {
	case FTW_NS:
		warn("%s: %s", fn);
		break;
	case FTW_F:
		ptype = PFWT_F;
		if (0)
		/* FALLTHROUGH */
	case FTW_D:
		ptype = PFWT_D;
		if (0)
		/* FALLTHROUGH */
	case FTW_DP:
		ptype = PFWT_DP;
		if (realpath(fn, buf) == NULL)
			warn("%s", fn);
		else {
			if (wflags & PFL_FILEWALKF_VERBOSE)
				warnx("processing %s%s",
				    buf, flags == FTW_F ? "" : "/");
			rc = pfl_filewalk_cbf(buf, stb, ptype,
			    ftw->level, pfl_filewalk_arg);
			if (rc == PFL_FILEWALK_RC_SKIP)
				ftw->__quit = FTW_PRUNE;
			else if (rc)
				return (rc);
		}
		break;
	case FTW_SL:
		if (wflags & PFL_FILEWALKF_VERBOSE)
			warnx("processing %s", fn);
		rc = pfl_filewalk_cbf(fn, stb, PFWT_SL, ftw->level,
		    pfl_filewalk_arg);
		if (rc == PFL_FILEWALK_RC_SKIP)
			ftw->__quit = FTW_PRUNE;
		if (rc)
			return (rc);
		break;
	default:
		warn("%s: %s", fn);
		break;
	}
	return (0);
}

int
pfl_filewalk(const char *fn, int flags,
    int (*cmpf)(const FTSENT **, const FTSENT **),
    int (*cbf)(const char *, const struct stat *, int, int, void *),
    void *arg)
{
	char buf[PATH_MAX];
	struct stat stb;
	int rc = 0;

	if (flags & PFL_FILEWALKF_RECURSIVE) {
		static psc_spinlock_t pfl_filewalk_lock = SPINLOCK_INIT;

		spinlock(&pfl_filewalk_lock);
		pfl_filewalk_cbf = cbf;
		pfl_filewalk_arg = arg;
		rc = nftw(fn, pfl_filewalk_cb, 1024, FTW_PHYS);
		freelock(&pfl_filewalk_lock);
		if (rc)
			psc_fatal("open %s", fn);
	} else {
		if (lstat(fn, &stb) == -1)
			err(1, "%s", fn);
		else if (!S_ISREG(stb.st_mode) && !S_ISDIR(stb.st_mode))
			errx(1, "%s: not a file or directory", fn);
		else if (realpath(fn, buf) == NULL)
			err(1, "%s", fn);
		else {
			int info;

			info = pfl_filewalk_stm2info(stb.st_mode);
			rc = cbf(buf, &stb, info, 0, arg);
//			if (info == FTS_D)
//				rc = cbf(buf, &stb, FTS_DP, 0, arg);
		}
	}
	return (rc);
}
#endif
