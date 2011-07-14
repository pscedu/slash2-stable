/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_AIO_H_
#define _PFL_AIO_H_

#ifdef HAVE_AIO
#  include <aio.h>
#else
#  include <sys/types.h>

#  include "pfl/cdefs.h"

struct aiocb {
	int	 aio_fildes;
	off_t	 aio_offset;
	void	*aio_buf;
	size_t	 aio_nbytes;
};

static __inline int
aio_return(__unusedx struct aiocb *aio)
{
	errno = ENOTSUP;
	psc_fatal("error");
}

static __inline int
aio_error(__unusedx struct aiocb *aio)
{
	errno = ENOTSUP;
	psc_fatal("error");
}

static __inline int
aio_read(__unusedx struct aiocb *aio)
{
	errno = ENOTSUP;
	psc_fatal("error");
}
#endif

#endif /* _PFL_AIO_H_ */
