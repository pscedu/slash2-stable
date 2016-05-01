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

#ifdef HAVE_LIBPTHREAD

#include "pfl/alloc.h"
#include "pfl/thread.h"
#include "pfl/usklndthr.h"

void
psc_usklndthr_begin(struct psc_thread *thr)
{
	struct psc_usklndthr *put;

	put = thr->pscthr_private;
	put->put_startf(put->put_arg);
}

int
cfs_create_thread(cfs_thread_t startf, void *arg,
    const char *namefmt, ...)
{
	char name[PSC_THRNAME_MAX];
	struct psc_usklndthr *put;
	struct psc_thread *thr;
	va_list ap;

	va_start(ap, namefmt);
	psc_usklndthr_get_namev(name, namefmt, ap);
	va_end(ap);

	thr = pscthr_init(psc_usklndthr_get_type(namefmt),
	    psc_usklndthr_begin, sizeof(*put), name);
	put = thr->pscthr_private;
	put->put_startf = startf;
	put->put_arg = arg;
	pscthr_setready(thr);
	return (0);
}

#endif
