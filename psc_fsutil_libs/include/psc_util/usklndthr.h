/* $Id$ */

#include <stdarg.h>

#include "libcfs/libcfs.h"

struct psc_usklndthr {
	cfs_thread_t	 put_startf;
	void		*put_arg;
};

void	psc_usklndthr_get_namev(char [], const char *, va_list);
int	psc_usklndthr_get_type(const char*);
