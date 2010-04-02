/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _PFL_NET_H_
#define _PFL_NET_H_

#include <sys/socket.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#elif __BYTE_ORDER != __BIG_ENDIAN
# error "unknown system"
#endif

#ifdef MSG_NOSIGNAL
# define PFL_MSG_NOSIGNAL MSG_NOSIGNAL
#else
# define PFL_MSG_NOSIGNAL 0
#endif

void pfl_socket_setnosig(int);

#endif /* _PFL_NET_H_ */
