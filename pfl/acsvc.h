/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_ACSVC_H_
#define _PFL_ACSVC_H_

struct psc_thread;

enum {
	ACSOP_ACCESS,
	ACSOP_CHMOD,
	ACSOP_CHOWN,
	ACSOP_LINK,
	ACSOP_LSTAT,
	ACSOP_MKDIR,
	ACSOP_MKNOD,
	ACSOP_OPEN,
	ACSOP_READLINK,
	ACSOP_RENAME,
	ACSOP_RMDIR,
	ACSOP_STAT,
	ACSOP_STATFS,
	ACSOP_SYMLINK,
	ACSOP_TRUNCATE,
	ACSOP_UNLINK,
	ACSOP_UTIMES
};

struct psc_thread *
	acsvc_init(int, const char *, char **);
int	access_fsop(int, uid_t, gid_t, const char *, ...);

#endif /* _PFL_ACSVC_H_ */
