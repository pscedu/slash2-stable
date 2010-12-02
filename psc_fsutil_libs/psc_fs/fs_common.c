/* $Id$ */

#include <stdio.h>
#include <string.h>

#include <fuse.h>

#include "pfl/fs.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlsvr.h"

#ifndef PFL_NO_CTL
int
pscfs_ctlparam(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels)
{
	char nbuf[30];
	int set;

	if (nlevels > 2)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	levels[0] = "fuse";

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set && nlevels != 2)
		return (psc_ctlsenderr(fd, mh, "invalid operation"));

#ifdef HAVE_FUSE_DEBUGLEVEL
	if (nlevels < 2 || strcmp(levels[1], "debug") == 0) {
		if (set) {
			char *endp;
			long val;

			endp = NULL;
			val = strtol(pcp->pcp_value, &endp, 10);
			if (val < 0 || val > 1 ||
			    endp == pcp->pcp_value || *endp != '\0')
				return (psc_ctlsenderr(fd, mh,
				    "invalid fuse.debug value: %s",
				    pcp->pcp_value));
			pscfs_setdebug(val);
		} else {
			int val;

			pscfs_getdebug(&val);
			levels[1] = "debug";
			snprintf(nbuf, sizeof(nbuf), "%d", val);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 2, nbuf))
				return (0);
		}
	}
#endif
	if (nlevels < 2 || strcmp(levels[1], "version") == 0) {
		if (set)
			goto readonly;
		levels[1] = "version";
		snprintf(nbuf, sizeof(nbuf), "%d.%d",
		    FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
		if (!psc_ctlmsg_param_send(fd, mh, pcp,
		    PCTHRNAME_EVERYONE, levels, 2, nbuf))
			return (0);
	}
	return (1);

 readonly:
	return (psc_ctlsenderr(fd, mh,
	    "field %s is read-only", levels[1]));
}
#endif
