/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2013, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _LIBCFS_SOLARIS_KP30_H_
#define _LIBCFS_SOLARIS_KP30_H_

#include <sys/ioccom.h>

#include <inttypes.h>

#define CFS_MODULE_PARM(name, t, type, perm, desc)

#define IOCTL_LIBCFS_TYPE long

#define LPU64 "%"PRIu64
#define LPD64 "%"PRId64
#define LPX64 "%"PRIx64
#define LPSZ  "%lu"
#define LPSSZ "%ld"

# define LI_POISON ((int)0x5a5a5a5a)
# define LL_POISON ((long)0x5a5a5a5a)
# define LP_POISON ((void *)(long)0x5a5a5a5a)

#endif /* _LIBCFS_SOLARIS_KP30_H_ */
