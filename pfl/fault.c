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

#define PSC_FAULT_NBUCKETS	16

static	int		fault_enabled = 0;

atomic_t		psc_fault_count;
struct psc_hashtbl	psc_fault_table;

void
psc_faults_init(void)
{
	atomic_set(&psc_fault_count, 0);

	psc_hashtbl_init(&psc_fault_table, PHTF_STR, struct psc_fault,
	    pflt_name, pflt_hentry, PSC_FAULT_NBUCKETS, NULL, "faults");
}

int
psc_fault_add(const char *name)
{
	int			 i;
	struct psc_hashbkt	*b;
	int			 rc;
	struct psc_fault	*pflt;

	if (strlen(name) >= sizeof(pflt->pflt_name))
		return (ENAMETOOLONG);

	i = 0;
	rc = 0;
	for (i = 0; psc_fault_names[i]; i++) {
		if (psc_fault_names[i] == NULL)
			return (ENOENT);
		if (strcmp(name, psc_fault_names[i]) == 0)
			break;
	}

	pflt = PSCALLOC(sizeof(*pflt));
	INIT_SPINLOCK(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, name, sizeof(pflt->pflt_name));
	pflt->pflt_flags = PFLTF_ACTIVE;
	pflt->pflt_delay = 0;		/* no internal delay enforced */
	pflt->pflt_begin = 0;		/* wait zero time before triggered */
	pflt->pflt_chance = 100;	/* alway happens */
	pflt->pflt_count = 1;		/* one time only */
	pflt->pflt_retval = 0;		/* keep the original error code */

	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	if (psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name)) {
		rc = EEXIST;
		PSCFREE(pflt);
	} else {
		fault_enabled = 1;
		atomic_inc(&psc_fault_count);
		psc_hashbkt_add_item(&psc_fault_table, b, pflt);
	}
	psc_hashbkt_unlock(b);
	return (rc);
}

int
psc_fault_remove(const char *name)
{
	struct psc_fault *pflt;
	struct psc_hashbkt *b;
	int rc;

	rc = ENOENT;
	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	pflt = psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name);
	if (pflt) {
		rc = 0;
		atomic_dec(&psc_fault_count);
		psc_hashent_remove(&psc_fault_table, pflt);
		PSCFREE(pflt);
	}
	psc_hashbkt_unlock(b);
	return (rc);
}

void
psc_fault_take_lock(void *p)
{
	struct psc_fault *pflt;

	pflt = p;
	psc_fault_lock(pflt);
}


struct psc_fault *
psc_fault_lookup(const char *name)
{
	struct psc_fault *pflt;

	pflt = psc_hashtbl_search(&psc_fault_table, NULL, psc_fault_take_lock, name);
	return (pflt);
}

/*
 * Alternative to add().  This function allows us to set fault points even before control thread
 * is ready to receive commands.
 */
int
psc_fault_register(const char *name, int delay, int begin, int chance, int count, int retval)
{
	int			 i;
	struct psc_hashbkt	*b;
	struct psc_fault	*pflt;

	if (strlen(name) >= sizeof(pflt->pflt_name))
		return (ENAMETOOLONG);

	for (i = 0; psc_fault_names[i]; i++) {
		if (psc_fault_names[i] == NULL)
			return (ENOENT);
		if (strcmp(name, psc_fault_names[i]) == 0)
			break;
	}

	pflt = PSCALLOC(sizeof(*pflt));
	INIT_SPINLOCK(&pflt->pflt_lock);
	strlcpy(pflt->pflt_name, name, sizeof(pflt->pflt_name));

	b = psc_hashbkt_get(&psc_fault_table, name);
	psc_hashbkt_lock(b);
	if (psc_hashbkt_search(&psc_fault_table, b, NULL, NULL, name)) {
		PSCFREE(pflt);
	} else {
		atomic_inc(&psc_fault_count);
		psc_hashbkt_add_item(&psc_fault_table, b, pflt);
	}
	pflt->pflt_flags = PFLTF_ACTIVE;
	pflt->pflt_delay = delay;		/* no internal delay enforced */
	pflt->pflt_begin = begin;		/* wait zero time before triggered */
	pflt->pflt_chance = chance;		/* alway happens */
	pflt->pflt_count = count;		/* one time only */
	pflt->pflt_retval = retval;		/* keep the original error code */
	psc_hashbkt_unlock(b);
	fault_enabled = 1;
	return (0);
}

void
psc_fault_here(const char *name, int *rc)
{
	struct psc_fault	*pflt;
	int			 dice;

	if (!fault_enabled)
		return;

	pflt = psc_hashtbl_search(&psc_fault_table,
	    NULL, psc_fault_take_lock, name);
	if (pflt == NULL)
		return;

	if (!(pflt->pflt_flags & PFLTF_ACTIVE)) {
		psc_fault_unlock(pflt);
		return;
	}
	if (pflt->pflt_unhits < pflt->pflt_begin) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	if (pflt->pflt_hits >= pflt->pflt_count) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	dice = psc_random32u(100);
	if (dice > pflt->pflt_chance) {
		pflt->pflt_unhits++;
		psc_fault_unlock(pflt);
		return;
	}
	pflt->pflt_hits++;
	if (pflt->pflt_delay)
		usleep(pflt->pflt_delay);
	if (pflt->pflt_retval)
		*rc = pflt->pflt_retval;
	psc_fault_unlock(pflt);
}

/*
 * psc_ctlrep_getfault - send a response to a "getfault" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @pcpl: control message to examine and reuse.
 */
int
psc_ctlrep_getfault(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	struct psc_ctlmsg_fault *pcflt = msg;
	struct psc_fault *pflt;
	struct psc_hashbkt *b;
	char name[PSC_FAULT_NAME_MAX];
	int rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pcflt->pcflt_name, sizeof(name));
	all = (name[0] == '\0');
	PSC_HASHTBL_LOCK(&psc_fault_table);
	PSC_HASHTBL_FOREACH_BUCKET(b, &psc_fault_table) {
		psc_hashbkt_lock(b);
		PSC_HASHBKT_FOREACH_ENTRY(&psc_fault_table, pflt, b) {
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
		}
		psc_hashbkt_unlock(b);
		if (!rc)
			break;
	}
	PSC_HASHTBL_ULOCK(&psc_fault_table);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown fault point: %s", name);
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
			snprintf(nbuf, sizeof(nbuf), "%ld", pflt->pflt_delay);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_count);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_begin);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_chance);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_retval);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_hits);
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
			snprintf(nbuf, sizeof(nbuf), "%d", pflt->pflt_unhits);
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
	struct psc_hashbkt *b;
	int rc, set;
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
		if (nlevels == 1) {
			if (pcp->pcp_flags & PCPF_ADD) {
				rc = psc_fault_add(pcp->pcp_value);
				if (rc == EEXIST)
					return (psc_ctlsenderr(fd, mh,
					    "fault point already exists"));
				else if (rc)
					return (psc_ctlsenderr(fd, mh,
					    "error adding fault: %s",
					    strerror(rc)));
			} else if (pcp->pcp_flags & PCPF_SUB) {
				rc = psc_fault_remove(pcp->pcp_value);
				if (rc == ENOENT)
					return (psc_ctlsenderr(fd, mh,
					    "fault point does not exist"));
				else if (rc)
					return (psc_ctlsenderr(fd, mh,
					    "error removing fault: %s",
					    strerror(rc)));
			} else
				return (psc_ctlsenderr(fd, mh,
				    "invalid operation"));
			return (1);
		} else if (nlevels != 3)
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
		PSC_HASHTBL_LOCK(&psc_fault_table);
		PSC_HASHTBL_FOREACH_BUCKET(b, &psc_fault_table) {
			psc_hashbkt_lock(b);
			PSC_HASHBKT_FOREACH_ENTRY(&psc_fault_table, pflt, b) {
				psc_fault_lock(pflt);
				rc = psc_ctlparam_faults_handle(fd, mh,
				    pcp, levels, nlevels, pflt, val);
				psc_fault_unlock(pflt);
				if (!rc)
					break;
			}
			psc_hashbkt_unlock(b);
			if (!rc)
				break;
		}
		PSC_HASHTBL_ULOCK(&psc_fault_table);
	} else {
		pflt = psc_fault_lookup(levels[1]);
		if (pflt == NULL)
			return (psc_ctlsenderr(fd, mh,
			    "invalid fault point: %s", levels[1]));
		rc = psc_ctlparam_faults_handle(fd, mh,
		    pcp, levels, nlevels, pflt, val);
		psc_fault_unlock(pflt);
	}
	return (rc);
}
