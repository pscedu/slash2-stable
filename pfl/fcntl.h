/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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
