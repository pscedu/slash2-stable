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

/*
 * Subsystem definitions.
 * Subsystems are used to modularize components of an
 * application for pinpointing error messages, etc.
 */

#ifndef _PFL_SUBSYS_H_
#define _PFL_SUBSYS_H_

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

extern int psc_nsubsys;

#endif /* _PFL_SUBSYS_H_ */
