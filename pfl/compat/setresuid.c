/* $Id$ */

#include <sys/types.h>

#include <unistd.h>

#include "pfl/setresuid.h"

int
setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	int rc;

	rc = setreuid(suid, euid);
	if (rc)
		return (rc);
	return (setreuid(ruid, euid));
}

int
setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	int rc;

	rc = setregid(sgid, egid);
	if (rc)
		return (rc);
	return (setregid(rgid, egid));
}
