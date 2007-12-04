/* $Id: subsys.h 1868 2007-10-12 18:52:11Z yanovich $ */

/*
 * zestiond subsystem definitions.
 */

#ifndef _SUBSYS_H_
#define _SUBSYS_H_

/* Must stay sync'd with subsys_names[] in subsys.c. */
#define ZS_ADDRCACHE	0
#define ZS_CHUNKMAP	1
#define ZS_FILEOPS	2
#define ZS_INODE	3
#define ZS_LOG		4
#define ZS_READ		5
#define ZS_SYNC		6
#define ZS_CIOD		7
#define ZS_RPC		8
#define ZS_LNET		9
#define ZS_PARITY	10
#define ZS_OTHER	11
#define ZNSUBSYS	12
#define NSUBSYS ZNSUBSYS

int		 zsubsys_id(const char *);
const char	*zsubsys_name(int);

#endif /* _SUBSYS_H_ */
