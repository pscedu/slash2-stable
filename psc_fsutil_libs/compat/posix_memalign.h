/* $Id$ */

#ifndef _PFL_COMPAT_POSIX_MEMALIGN_H_
#define _PFL_COMPAT_POSIX_MEMALIGN_H_

#include <sys/types.h>

int	posix_memalign(void **, size_t, size_t);
void	psc_freen(void *);

#endif /* _PFL_COMPAT_POSIX_MEMALIGN_H_ */
