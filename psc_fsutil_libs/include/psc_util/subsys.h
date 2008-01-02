/* $Id$ */

/*
 * Subsystem definitions.
 * Subsystems are used to modularize components of an
 * application for pinpointing error messages, etc.
 */

#ifndef _PFL_SUBSYS_H_
#define _PFL_SUBSYS_H_

#include "psc_ds/dynarray.h"

#define PSS_LOG		0
#define PSS_JOURNAL	1
#define PSS_RPC		2
#define PSS_LNET	3
#define PSS_OTHER	4
#define _PSS_LAST	5

int		 psc_subsys_id(const char *);
const char	*psc_subsys_name(int);
void		 psc_subsys_register(int, const char *);

extern struct dynarray psc_subsystems;
extern int psc_nsubsys;

#endif /* _PFL_SUBSYS_H_ */
