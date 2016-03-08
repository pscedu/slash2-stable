/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2016, Google, Inc.
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

#include <stdlib.h>
#include <string.h>

#include "pfl/cdefs.h"

const char *
sys_strerror(int rc)
{
	return (strerror(rc));
}

#include "pfl/err.h"

char *pfl_errstrs[] = {
/*  0 */ "Bad message",
/*  1 */ "Key has expired",
/*  2 */ "No connection to peer",
/*  3 */ "Operation already in progress",
/*  4 */ "Operation not supported",
/*  5 */ "Function not implemented",
/*  6 */ "Operation canceled",
/*  7 */ "Stale file handle",
/*  8 */ "Bad magic",
/*  9 */ "Required key not available",
/* 10 */ "Invalid checksum",
/* 11 */ "Operation timed out",
/* 12 */ "Resource temporarily unavailable",
	  NULL
};

const char *
pfl_strerror(int error)
{
	error = abs(error);

	if (error >= _PFLERR_START &&
	    error < _PFLERR_START + nitems(pfl_errstrs))
		return (pfl_errstrs[error - _PFLERR_START]);
	return (sys_strerror(error));
}
