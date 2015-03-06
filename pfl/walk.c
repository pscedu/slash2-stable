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
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "pfl/fts.h"
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
 * Traverse a file hierarchy and perform an operation on each file
 * system entry.
 * @fn: file root.
 * @flags: behavorial flags.
 * @cmpf: optional dirent comparator for ordering.
 * @cbf: callback to invoke on each file.
 * @arg: optional argument to supply to callback.
 * Notes: the callback will be invoked with a fully resolved absolute
 *	path name unless the file in question is a symbolic link.
 */
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
		fp = pfl_fts_open(pathv, f_flags | FTS_COMFOLLOW |
		    FTS_PHYSICAL, cmpf);
		if (fp == NULL)
			psc_fatal("fts_open %s", fn);
		while ((f = pfl_fts_read(fp)) != NULL) {
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
					pfl_fts_set(fp, f, FTS_SKIP);
				else if (rc) {
					pfl_fts_close(fp);
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
					pfl_fts_set(fp, f, FTS_SKIP);
				else if (rc) {
					pfl_fts_close(fp);
					return (rc);
				}
				break;
			default:
				psclog_warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			}
		}
		pfl_fts_close(fp);
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
