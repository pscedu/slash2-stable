/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_GETOPT_H_
#define _PFL_GETOPT_H_

#include <unistd.h>

#define PFL_OPT_RESET()							\
	do {								\
		optind = 1;						\
		optreset = 1;						\
	} while (0)

#ifndef HAVE_OPTRESET
int optreset;
#endif

#endif /* _PFL_COPYRIGHT_H_ */
