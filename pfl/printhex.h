/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_PRINTHEX_H_
#define _PFL_PRINTHEX_H_

#include <stdio.h>

void pfl_unpack_hex(const void *, size_t, char *);

void fprinthex(FILE *, const void *, size_t);
void printvbin(const void *, size_t);
void printvbinr(const void *, size_t);

#define printhex(p, len)	fprinthex(stderr, (p), (len))

static __inline void
printbin(uint64_t val)
{
	printvbinr(&val, sizeof(val));
}

#endif /* _PFL_PRINTHEX_H_ */
