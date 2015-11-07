/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

/*
 * Subsystem definitions.
 * Subsystems are used to modularize components of an
 * application for pinpointing error messages, etc.
 */

#ifndef _PFL_SUBSYS_H_
#define _PFL_SUBSYS_H_

#include "pfl/dynarray.h"

#define PSS_ALL		(-1)
#define PSS_DEF		0		/* default */
#define PSS_TMP		1		/* temporary debug use */
#define PSS_MEM		2
#define PSS_LNET	3
#define PSS_RPC		4
#define _PSS_LAST	5

int		 psc_subsys_id(const char *);
const char	*psc_subsys_name(int);
void		 psc_subsys_register(int, const char *);

extern struct psc_dynarray psc_subsystems;

#endif /* _PFL_SUBSYS_H_ */
