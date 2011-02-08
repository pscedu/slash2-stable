/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_SYS_H_
#define _PFL_SYS_H_

#include <sys/types.h>

int	pflsys_getusergroups(uid_t, gid_t, gid_t **, int *);
int	pflsys_userisgroupmember(uid_t, gid_t, gid_t);

#endif /* _PFL_SYS_H_ */
