/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-2013, Pittsburgh Supercomputing Center (PSC).
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

#undef _FILE_OFFSET_BITS	/* FTS is not 64-bit ready */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>

#ifdef HAVE_FTS
#include <fts.h>
#else
#include <ftw.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "pfl/str.h"
#include "pfl/walk.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

int
pfl_filewalk_stm2info(int mode)
{
	if (S_ISREG(mode))
		return (FTS_F);
	if (S_ISDIR(mode))
		return (FTS_D);
	psc_fatalx("invalid mode %#o", mode);
}

/**
 * pfl_filewalk - Traverse a file hierarchy with a given operation.
 * @fn: file root
 * @flags: behavorial flags.
 * @cbf: callback to invoke on each file.
 * @arg: optional argument to supply to callback.
 * Notes: the callback will be invoked with a fully resolved absolute
 *	path name unless the file in question is a symbolic link.
 */
#ifdef HAVE_FTS
int
pfl_filewalk(const char *fn, int flags, int (*cbf)(const char *,
    const struct stat *, int, int, void *), void *arg)
{
	char * const pathv[] = { (char *)fn, NULL };
	char buf[PATH_MAX];
	struct stat stb;
	int rc = 0;
	FTSENT *f;
	FTS *fp;

	if (flags & PFL_FILEWALKF_RECURSIVE) {
		/* XXX security implications of FTS_NOCHDIR? */
		fp = fts_open(pathv, FTS_COMFOLLOW | FTS_NOCHDIR |
		    FTS_PHYSICAL, NULL);
		if (fp == NULL)
			psc_fatal("fts_open %s", fn);
		while ((f = fts_read(fp)) != NULL) {
			switch (f->fts_info) {
			case FTS_NS:
				warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			case FTS_F:
			case FTS_D:
				if (realpath(f->fts_path, buf) == NULL)
					warn("%s", f->fts_path);
				else {
					if (flags & PFL_FILEWALKF_VERBOSE)
						warnx("processing %s%s",
						    buf, f->fts_info ==
						    FTS_D ? "/" : "");
					rc = cbf(buf, f->fts_statp,
					    f->fts_info, f->fts_level,
					    arg);
					if (rc == PFL_FILEWALK_RC_SKIP)
						fts_set(fp, f, FTS_SKIP);
					else if (rc) {
						fts_close(fp);
						return (rc);
					}
				}
				break;
			case FTS_SL:
				if (flags & PFL_FILEWALKF_VERBOSE)
					warnx("processing %s", f->fts_path);
				rc = cbf(f->fts_path, f->fts_statp,
				    f->fts_info, f->fts_level, arg);
				if (rc == PFL_FILEWALK_RC_SKIP)
					fts_set(fp, f, FTS_SKIP);
				else if (rc) {
					fts_close(fp);
					return (rc);
				}
				break;
			case FTS_DP:
				break;
			default:
				warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			}
		}
		fts_close(fp);
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
		}
	}
	return (rc);
}

#else

int (*pfl_filewalk_cbf)(const char *, const struct stat *, int, int,
    void *);
void *pfl_filewalk_arg;

int
pfl_filewalk_cb(const char *fn, const struct stat *stb, int flags,
    struct FTW *ftw)
{
	char buf[PATH_MAX];
	int rc;

	switch (flags) {
	case FTW_NS:
		warn("%s: %s", fn);
		break;
	case FTW_F:
	case FTW_D:
		if (realpath(fn, buf) == NULL)
			warn("%s", fn);
		else {
			if (flags & PFL_FILEWALKF_VERBOSE)
				warnx("processing %s%s",
				    buf, flags == FTW_D ? "/" : "");
			rc = pfl_filewalk_cbf(buf, stb, flags,
			    ftw->level, pfl_filewalk_arg);
			if (rc == PFL_FILEWALK_RC_SKIP)
				ftw->__quit = FTW_PRUNE;
			else if (rc)
				return (rc);
		}
		break;
	case FTW_SL:
		if (flags & PFL_FILEWALKF_VERBOSE)
			warnx("processing %s", fn);
		rc = pfl_filewalk_cbf(fn, stb, flags, ftw->level,
		    pfl_filewalk_arg);
		if (rc == PFL_FILEWALK_RC_SKIP)
			ftw->__quit = FTW_PRUNE;
		if (rc)
			return (rc);
		break;
	case FTW_DP:
		break;
	default:
		warn("%s: %s", fn);
		break;
	}
	return (0);
}

int
pfl_filewalk(const char *fn, int flags, int (*cbf)(const char *,
    const struct stat *, int, int, void *), void *arg)
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
