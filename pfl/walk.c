/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2008-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/stat.h>

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "pfl/alloc.h"
#include "pfl/fts.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/str.h"
#include "pfl/walk.h"

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
pfl_filewalk(const char *fn, int flags, void *cmpf, int (*cbf)(FTSENT *,
    void *), void *arg)
{
	char * const pathv[] = { (char *)fn, NULL };
	int rc = 0, f_flags = 0;
	struct stat stb;
	FTSENT *f;
	FTS *fp;

	if (flags & PFL_FILEWALKF_RECURSIVE) {
		if (flags & PFL_FILEWALKF_NOSTAT)
			f_flags |= FTS_NOSTAT;
		if (flags & PFL_FILEWALKF_NOCHDIR)
			f_flags |= FTS_NOCHDIR;
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
			case FTS_D:
			case FTS_SL:
				if (flags & PFL_FILEWALKF_VERBOSE)
					warnx("processing %s%s",
					    fn, f->fts_info == FTS_D ?
					    "/" : "");
			case FTS_DP:
				rc = cbf(f, arg);
				if (rc) {
					pfl_fts_close(fp);
					return (rc);
				}
				break;
			default:
				if (f->fts_errno == 0)
					f->fts_errno = EOPNOTSUPP;
				psclog_warnx("%s: %s", f->fts_path,
				    strerror(f->fts_errno));
				break;
			}
		}
		pfl_fts_close(fp);
	} else {
		const char *basefn;
		size_t baselen;

		if (lstat(fn, &stb) == -1)
			err(1, "%s", fn);
		basefn = pfl_basename(fn);
		baselen = strlen(basefn);

		f = PSCALLOC(sizeof(*f) + baselen);
		f->fts_accpath = (char *)fn;
		f->fts_path = (char *)fn;
		f->fts_pathlen = strlen(fn);
		strlcpy(f->fts_name, basefn, baselen + 1);
		f->fts_namelen = baselen;
		f->fts_ino = stb.st_ino;
		f->fts_statp = &stb;
		switch (stb.st_mode & S_IFMT) {
		case S_IFDIR: f->fts_info = FTS_D; break;
		case S_IFREG: f->fts_info = FTS_F; break;
		case S_IFLNK: f->fts_info = FTS_SL; break;
		case S_IFBLK: f->fts_info = FTS_DEFAULT; break;
		default:
			psclog_warnx("%s: %s", fn,
			    strerror(EOPNOTSUPP));
			break;
		}
		rc = cbf(f, arg);
		PSCFREE(f);
	}
	return (rc);
}
