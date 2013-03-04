/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2009-2013, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Routines for applying operations to all files inside file system
 * hierarchies.
 */

#ifndef _PFL_FILEWALK_H_
#define _PFL_FILEWALK_H_

struct stat;

int pfl_filewalk(const char *, int, int (*)(const char *,
	const struct stat *, void *), void *);

#define PFL_FILEWALKF_VERBOSE	(1 << 0)
#define PFL_FILEWALKF_RECURSIVE	(1 << 1)

#endif /* _PFL_FILEWALK_H_ */
