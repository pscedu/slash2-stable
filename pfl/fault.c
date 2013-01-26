/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/str.h"
#include "pfl/hashtbl.h"
#include "psc_util/alloc.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlsvr.h"
#include "psc_util/fault.h"
#include "psc_util/lock.h"
#include "psc_util/random.h"

#define			FAULTS_MAX 128

struct psc_fault	psc_faults[FAULTS_MAX];
int			psc_nfaults;

struct psc_fault *
psc_fault_lookup(const char *name)
{
	struct psc_fault *pflt;
	int i;

	for (i = 0, pflt = psc_faults; i < psc_nfaults; i++, pflt++)
		if (strcmp(pflt->pflt_name, name) == 0)
			return (pflt);
	return (NULL);
}

/**
 * psc_fault_register -
 */
struct psc_fault *
_psc_fault_register(const char *name)
{
	struct psc_fault *pflt;
	char *p;

	psc_assert(strlen(name) < sizeof(pflt->pflt_name));

	pflt = psc_fault_lookup(name);
	psc_assert(pflt == NULL);

	p = strstr(name, "_FAULT_");
	psc_assert(p);

	pflt = &psc_faults[psc_nfaults++];
	INIT_SPINLOCK(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, p + 7, sizeof(pflt->pflt_name));
	for (p = pflt->pflt_name; *p; p++)
		*p = tolower(*p);
	pflt->pflt_chance = 100;
	pflt->pflt_count = -1;
	return (pflt);
}

int
_psc_fault_here(struct psc_fault *pflt, int *rcp, int rc)
{
	long delay = 0, faulted = 0;

	psc_fault_lock(pflt);
	if (pflt->pflt_unhits < pflt->pflt_begin)
		goto out;
	if (pflt->pflt_count >= 0 &&
	    pflt->pflt_hits >= pflt->pflt_count)
		goto out;
	if (pflt->pflt_chance < (int)psc_random32u(100))
		goto out;
	if (rc)
		*rcp = rc;
	else if (pflt->pflt_retval)
		*rcp = pflt->pflt_retval;
	pflt->pflt_hits++;
	delay = pflt->pflt_delay;
	faulted = 1;
	if (0)
 out:
		pflt->pflt_unhits++;
	psc_fault_unlock(pflt);

	if (delay)
		usleep(delay);
	return (faulted);
}

/**
 * psc_ctlrep_getfault - Send a response to a "getfault" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 */
int
psc_ctlrep_getfault(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	struct psc_ctlmsg_fault *pcflt = msg;
	struct psc_fault *pflt;
	char name[PSC_FAULT_NAME_MAX];
	int i, rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pcflt->pcflt_name, sizeof(name));
	all = (name[0] == '\0');
	for (i = 0, pflt = psc_faults; i < psc_nfaults; i++, pflt++)
		if (all || strncmp(pflt->pflt_name, name,
		    strlen(name)) == 0) {
			found = 1;

			psc_fault_lock(pflt);
			strlcpy(pcflt->pcflt_name, pflt->pflt_name,
			    sizeof(pcflt->pcflt_name));
			pcflt->pcflt_flags = pflt->pflt_flags;
			pcflt->pcflt_hits = pflt->pflt_hits;
			pcflt->pcflt_unhits = pflt->pflt_unhits;
			pcflt->pcflt_delay = pflt->pflt_delay;
			pcflt->pcflt_count = pflt->pflt_count;
			pcflt->pcflt_begin = pflt->pflt_begin;
			pcflt->pcflt_retval = pflt->pflt_retval;
			pcflt->pcflt_chance = pflt->pflt_chance;
			psc_fault_unlock(pflt);

			rc = psc_ctlmsg_sendv(fd, mh, pcflt);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(pflt->pflt_name, name) == 0)
				break;
		}
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown fault point: %s",
		    name);
	return (rc);
}

int
psc_ctlparam_faults_handle(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    struct psc_fault *pflt, int val)
{
	char nbuf[20];
	int set;

	levels[1] = pflt->pflt_name;

	set = (mh->mh_type == PCMT_SETPARAM);

	if (nlevels < 3 || strcmp(levels[2], "active") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
				return (psc_ctlsenderr(fd, mh,
				    "invalid operation"));
			if (val)
				pflt->pflt_flags |= PFLTF_ACTIVE;
			else
				pflt->pflt_flags &= ~PFLTF_ACTIVE;
		} else {
			levels[2] = "active";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_flags & PFLTF_ACTIVE ? 1 : 0);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "delay") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_delay += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_delay -= val;
			else
				pflt->pflt_delay = val;
		} else {
			levels[2] = "delay";
			snprintf(nbuf, sizeof(nbuf), "%ld",
			    pflt->pflt_delay);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "count") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_count += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_count -= val;
			else
				pflt->pflt_count = val;
		} else {
			levels[2] = "count";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_count);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "begin") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_begin += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_begin -= val;
			else
				pflt->pflt_begin = val;
		} else {
			levels[2] = "begin";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_begin);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "chance") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_chance += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_chance -= val;
			else
				pflt->pflt_chance = val;
		} else {
			levels[2] = "chance";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_chance);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "retval") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_retval += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_retval -= val;
			else
				pflt->pflt_retval = val;
		} else {
			levels[2] = "retval";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_retval);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "hits") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_hits += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_hits -= val;
			else
				pflt->pflt_hits = val;
		} else {
			levels[2] = "hits";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_hits);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "unhits") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_unhits += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_unhits -= val;
			else
				pflt->pflt_unhits = val;
		} else {
			levels[2] = "unhits";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_unhits);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	return (1);
}

int
psc_ctlparam_faults(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	struct psc_fault *pflt;
	int i, rc, set;
	char *endp;
	long val;

	if (nlevels > 3)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	rc = 1;
	levels[0] = "faults";
	val = 0; /* gcc */

	/* sanity check field name */
	if (nlevels == 3 &&
	    strcmp(levels[2], "active") != 0 &&
	    strcmp(levels[2], "begin")  != 0 &&
	    strcmp(levels[2], "chance") != 0 &&
	    strcmp(levels[2], "count")  != 0 &&
	    strcmp(levels[2], "delay")  != 0 &&
	    strcmp(levels[2], "hits")   != 0 &&
	    strcmp(levels[2], "unhits") != 0 &&
	    strcmp(levels[2], "retval") != 0)
		return (psc_ctlsenderr(fd, mh,
		    "invalid faults field: %s", levels[2]));

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (nlevels != 3)
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));

		endp = NULL;
		val = strtol(pcp->pcp_value, &endp, 10);
		if (val == LONG_MIN || val == LONG_MAX ||
		    val > INT_MAX || val < 0 ||
		    endp == pcp->pcp_value || *endp != '\0')
			return (psc_ctlsenderr(fd, mh,
			    "invalid fault point %s value: %s",
			    levels[2], pcp->pcp_value));
	}

	if (nlevels == 1) {
		for (i = 0, pflt = psc_faults; i < psc_nfaults;
		    i++, pflt++) {
			psc_fault_lock(pflt);
			rc = psc_ctlparam_faults_handle(fd, mh, pcp,
			    levels, nlevels, pflt, val);
			psc_fault_unlock(pflt);
			if (!rc)
				break;
		}
	} else {
		pflt = psc_fault_lookup(levels[1]);
		if (pflt == NULL)
			return (psc_ctlsenderr(fd, mh,
			    "invalid fault point: %s", levels[1]));
		psc_fault_lock(pflt);
		rc = psc_ctlparam_faults_handle(fd, mh, pcp, levels,
		    nlevels, pflt, val);
		psc_fault_unlock(pflt);
	}
	return (rc);
}
