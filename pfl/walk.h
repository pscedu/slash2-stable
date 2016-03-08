/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2009-2015, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Routines for applying operations to all files inside file system
 * hierarchies.
 */

#ifndef _PFL_FILEWALK_H_
#define _PFL_FILEWALK_H_

#include "pfl/fts.h"

int pfl_filewalk(const char *, int, void *, int (*)(FTSENT *, void *),
    void *);

#define PFL_FILEWALKF_VERBOSE	(1 << 0)
#define PFL_FILEWALKF_RECURSIVE	(1 << 1)
#define PFL_FILEWALKF_NOSTAT	(1 << 2)
#define PFL_FILEWALKF_NOCHDIR	(1 << 3)

#endif /* _PFL_FILEWALK_H_ */
