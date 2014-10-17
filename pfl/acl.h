/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _PFL_ACL_H_
#define _PFL_ACL_H_

#ifdef HAVE_SYS_ACL_H_
#include <sys/acl.h>

acl_t pfl_acl_from_xattr(const void *, size_t);
#endif

#endif /* _PFL_ACL_H_ */
