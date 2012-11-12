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

#include <grp.h>
#include <limits.h> 
#include <pwd.h>

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
