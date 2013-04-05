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

#ifndef _PFL_FCNTL_H_
#define _PFL_FCNTL_H_

#include <fcntl.h>

#ifndef O_DSYNC
#define O_DSYNC		0
#endif

#ifndef O_RSYNC
#define O_RSYNC		0
#endif

#ifndef O_SHLOCK
#define O_SHLOCK	0
#endif

#ifndef O_EXLOCK
#define O_EXLOCK	0
#endif

#ifndef O_DIRECT
#define O_DIRECT	0
#endif

#ifndef O_SYMLINK
#define O_SYMLINK	0
#endif

#ifndef O_NOATIME
#define O_NOATIME	0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE	0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC	0
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY	0
#endif

#ifndef O_ASYNC
#define O_ASYNC		0
#endif

#endif /* _PFL_FCNTL_H_ */
