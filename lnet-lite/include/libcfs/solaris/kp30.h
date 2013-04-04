/* $Id$ */
/* %PSC_COPYRIGHT% */

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
