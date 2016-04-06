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

#include <stdint.h>
#include <string.h>

const char *
sys_strerror(int rc)
{
	return (strerror(rc));
}

#ifdef EKEYEXPIRED
# define PFLERR_KEYEXPIRED_STR	sys_strerror(EKEYEXPIRED)
#else
# define PFLERR_KEYEXPIRED_STR	"Key has expired"
#endif

#ifdef ENOKEY
# define PFLERR_NOKEY_STR	sys_strerror(ENOKEY)
#else
# define PFLERR_NOKEY_STR	"Required key not available"
#endif

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/err.h"
#include "pfl/hashtbl.h"
#include "pfl/str.h"

struct pfl_errno {
	struct pfl_hashentry	 hentry;
	const char		*str;
	uint64_t		 code;
};

struct psc_hashtbl pfl_errno_hashtbl;

void
pfl_register_errno(int code, const char *str)
{
	struct pfl_errno *e;
	uint64_t q;

	q = code;
	e = psc_hashtbl_search(&pfl_errno_hashtbl, &q);
	if (e) {
		psc_assert(e->code == q);
		psc_assert(strcmp(e->str, str) == 0);
		return;
	}

	e = PSCALLOC(sizeof(*e));
	e->code = q;
	e->str = pfl_strdup(str);
	psc_hashent_init(&pfl_errno_hashtbl, e);
	psc_hashtbl_add_item(&pfl_errno_hashtbl, e);
}

void
pfl_errno_init(void)
{
	psc_hashtbl_init(&pfl_errno_hashtbl, 0, struct pfl_errno, code,
	    hentry, 191, NULL, "errno");
}

const char *
pfl_strerror(int error)
{
	struct pfl_errno *e;
	uint64_t q;

#if DEBUG > 1
	psc_assert(error > 0);
#endif
	error = abs(error);

	switch (error) {
	case PFLERR_BADMSG:	return sys_strerror(EBADMSG);
	case PFLERR_KEYEXPIRED:	return PFLERR_KEYEXPIRED_STR;
	case PFLERR_NOTCONN:	return sys_strerror(ENOTCONN);
	case PFLERR_ALREADY:	return sys_strerror(EALREADY);
	case PFLERR_NOTSUP:	return sys_strerror(ENOTSUP);
	case PFLERR_NOSYS:	return sys_strerror(ENOSYS);
	case PFLERR_CANCELED:	return sys_strerror(ECANCELED);
	case PFLERR_STALE:	return sys_strerror(ESTALE);
	case PFLERR_BADMAGIC:	return "Bad magic";
	case PFLERR_NOKEY:	return PFLERR_NOKEY_STR;
	case PFLERR_BADCRC:	return "Bad checksum";
	case PFLERR_TIMEDOUT:	return sys_strerror(ETIMEDOUT);
	case PFLERR_WOULDBLOCK:	return sys_strerror(EWOULDBLOCK);
	}

	q = error;
	e = psc_hashtbl_search(&pfl_errno_hashtbl, &q);
	if (e)
		return (e->str);

	return (sys_strerror(error));
}
