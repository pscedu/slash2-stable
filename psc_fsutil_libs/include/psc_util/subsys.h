/* $Id$ */

/*
 * Subsystem definitions.
 * Subsystems are used to modularize components of an
 * application for pinpointing error messages, etc.
 */

#ifndef _PFL_SUBSYS_H_
#define _PFL_SUBSYS_H_

#define PSS_ALL		(-1)
#define PSS_LOG		0
#define PSS_JOURNAL	1
#define PSS_RPC		2
#define PSS_LNET	3
#define PSS_MEM		4
#define PSS_GEN		5		/* catchall */
#define PSS_TMP		6		/* temporary debug use */
#define _PSS_LAST	7

int		 psc_subsys_id(const char *);
const char	*psc_subsys_name(int);
void		 psc_subsys_register(int, const char *);

extern int psc_nsubsys;

#endif /* _PFL_SUBSYS_H_ */
