/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <grp.h>
#include <limits.h>
#include <pwd.h>

#include "pfl/alloc.h"
#include "pfl/str.h"
#include "pfl/sys.h"

int
pflsys_getusergroups(uid_t uid, gid_t defgid, gid_t *gv, int *ng)
{
	struct passwd pw, *pwp;
	char buf[LINE_MAX];
	int rc;

	*ng = 0;
	rc = getpwuid_r(uid, &pw, buf, sizeof(buf), &pwp);
	if (rc)
		return (rc);
	*ng = NGROUPS_MAX;
	getgrouplist(pw.pw_name, defgid, gv, ng);
	return (0);
}

int
pflsys_userisgroupmember(uid_t uid, gid_t defgid, gid_t gid)
{
	gid_t gv[NGROUPS_MAX];
	int j, ng;

	pflsys_getusergroups(uid, defgid, gv, &ng);
	for (j = 0; j < ng; j++)
		if (gid == gv[j])
			return (1);
	return (0);
}

int
pfl_systemf(const char *fmt, ...)
{
	va_list ap;
	char *buf;
	int rc;

	va_start(ap, fmt);
	pfl_vasprintf(&buf, fmt, ap);
	va_end(ap);

	rc = system(buf);

	free(buf);
	return (rc);
}
