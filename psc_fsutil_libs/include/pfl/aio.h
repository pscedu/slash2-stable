/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011, Pittsburgh Supercomputing Center (PSC).
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
