/* $Id$ */

#include <sys/types.h>

struct psc_thread;

enum {
	ACSOP_CHMOD,
	ACSOP_CHOWN,
	ACSOP_LINK,
	ACSOP_MKDIR,
	ACSOP_MKNOD,
	ACSOP_OPEN,
//	ACSOP_OPENDIR, /* XXX */
	ACSOP_READLINK,
	ACSOP_RENAME,
	ACSOP_RMDIR,
	ACSOP_STAT,
	ACSOP_STATFS,
	ACSOP_SYMLINK,
	ACSOP_TRUNCATE,
	ACSOP_UNLINK,
	ACSOP_UTIMES
};

void acsvc_init(struct psc_thread *, int, const char *);
int  access_fsop(int, uid_t, gid_t, const char *, ...);
