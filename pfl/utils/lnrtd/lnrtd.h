/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2011-2015, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _LNRTD_H_
#define _LNRTD_H_

#include "pfl/cdefs.h"

enum {
	LRTHRT_CTL,
	LRTHRT_CTLAC,
	LRTHRT_EQPOLL,
	LRTHRT_LNETAC,
	LRTHRT_OPSTIMER,
	LRTHRT_USKLNDPL
};

#if DEVELPATHS
# define PATH_RUNTIME_DIR	".."
#else
# define PATH_RUNTIME_DIR	"/var/run"
#endif

#define PATH_CTLSOCK PATH_RUNTIME_DIR"/lnrtd.%h.sock"

void lrctlthr_main(void);

const char *ctlsockfn;

#endif /* _LNRTD_H_ */
