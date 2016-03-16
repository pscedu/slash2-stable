/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2016, Google, Inc.
 * Copyright 2013-2015, Pittsburgh Supercomputing Center
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

#ifndef _PFL_ERR_H_
#define _PFL_ERR_H_

#include <string.h>

#define _PFLERR_START			500

#define PFLERR_BADMSG			(_PFLERR_START +  0)
#define PFLERR_KEYEXPIRED		(_PFLERR_START +  1)
#define PFLERR_NOTCONN			(_PFLERR_START +  2)
#define PFLERR_ALREADY			(_PFLERR_START +  3)
#define PFLERR_NOTSUP			(_PFLERR_START +  4)
#define PFLERR_NOSYS			(_PFLERR_START +  5)
#define PFLERR_CANCELED			(_PFLERR_START +  6)
#define PFLERR_STALE			(_PFLERR_START +  7)
#define PFLERR_BADMAGIC			(_PFLERR_START +  8)
#define PFLERR_NOKEY			(_PFLERR_START +  9)
#define PFLERR_BADCRC			(_PFLERR_START + 10)
#define PFLERR_TIMEDOUT			(_PFLERR_START + 11)
#define PFLERR_WOULDBLOCK		(_PFLERR_START + 12)

const char *
	pfl_strerror(int);

#ifndef PFL_USE_SYSTEM_STRERROR
#  define strerror(rc)			pfl_strerror(rc)
#endif

void	pfl_register_errno(int, const char *);
void	pfl_errno_init(void);

#endif /* _PFL_ERR_H_ */
