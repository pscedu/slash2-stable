/* $Id$ */

#include <sys/types.h>

#define P_NODIE 1	/* Don't die in getpidpath(). */

char	*getpidpath(char *, pid_t *, int);
int	 parsepid(char *, pid_t *);
