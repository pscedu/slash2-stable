/* $Id$ */
/* %PSC_COPYRIGHT% */

#include <pwd.h>
#include <grp.h>

#include "pfl/sys.h"
#include "psc_util/alloc.h"

int
pflsys_getusergroups(uid_t uid, gid_t defgid, gid_t **gvp, int *ng)
{
	struct passwd pw, *pwp;
	char buf[LINE_MAX];
	int rc;

	*ng = 0;

	rc = getpwuid_r(uid, &pw, buf, sizeof(buf), &pwp);
	if (rc)
		return (rc);

	rc = getgrouplist(pw.pw_name, defgid, NULL, ng);
	if (rc) {
		*gvp = psc_calloc(*ng, sizeof(**gvp), 0);
		getgrouplist(pw.pw_name, defgid, *gvp, ng);
	}
	return (rc);
}

int
pflsys_userisgroupmember(uid_t uid, gid_t defgid, gid_t gid)
{
	int j, ng, rc = 0;
	gid_t *gv;

	pflsys_getusergroups(uid, defgid, &gv, &ng);
	for (j = 0; j < ng; j++)
		if (gid == gv[j]) {
			rc = 1;
			break;
		}
	PSCFREE(gv);
	return (rc);
}
