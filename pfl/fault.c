/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright 2006-2016, Pittsburgh Supercomputing Center
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/ctl.h"
#include "pfl/ctlsvr.h"
#include "pfl/fault.h"
#include "pfl/hashtbl.h"
#include "pfl/lock.h"
#include "pfl/random.h"
#include "pfl/str.h"

struct psc_dynarray	pfl_faults;
psc_spinlock_t		pfl_faults_lock = SPINLOCK_INIT;

int
_pfl_fault_cmp(const void *a, const void *b)
{
	const struct pfl_fault *y = b;

	return (strcmp(a, y->pflt_name));
}

void
pfl_fault_destroy(int pos)
{
	struct pfl_fault *flt;
	int locked;

	locked = reqlock(&pfl_faults_lock);
	flt = psc_dynarray_getpos(&pfl_faults, pos);
	psc_dynarray_splice(&pfl_faults, pos, 1, NULL, 0);
	ureqlock(&pfl_faults_lock, locked);

	PSCFREE(flt);
}

struct pfl_fault *
_pfl_fault_get(int populate, const char *namefmt, ...)
{
	char name[PFL_FAULT_NAME_MAX];
	struct pfl_fault *flt = NULL;
	int pos, locked;
	va_list ap;

	va_start(ap, namefmt);
	vsnprintf(name, sizeof(name), namefmt, ap);
	va_end(ap);

	locked = reqlock(&pfl_faults_lock);
	pos = psc_dynarray_bsearch(&pfl_faults, name, _pfl_fault_cmp);
	if (pos < psc_dynarray_len(&pfl_faults)) {
		flt = psc_dynarray_getpos(&pfl_faults, pos);
		if (strcmp(flt->pflt_name, name) == 0)
			goto out;
	}
	if (!populate)
		goto out;
	flt = PSCALLOC(sizeof(*flt));
	INIT_SPINLOCK(&flt->pflt_lock);
	strlcpy(flt->pflt_name, name, sizeof(flt->pflt_name));
	flt->pflt_chance = 100;
	flt->pflt_count = -1;
	psc_dynarray_splice(&pfl_faults, pos, 0, &flt, 1);
 out:
	ureqlock(&pfl_faults_lock, locked);
	return (flt);
}

int
_pfl_fault_here(struct pfl_fault *pflt, int *rcp, int rc)
{
	long delay = 0, faulted = 0;

	pfl_fault_lock(pflt);

	if (pflt->pflt_unhits < pflt->pflt_begin)
		goto out;

	if (pflt->pflt_count >= 0 &&
	    pflt->pflt_hits >= pflt->pflt_count)
		goto out;

	/*
	 * Ignore probability if a count is explicitly given.
	 */
	if (pflt->pflt_count < 0 &&
	    pflt->pflt_chance < (int)psc_random32u(100))
		goto out;

	if (pflt->pflt_interval) {
		if (pflt->pflt_skips < pflt->pflt_interval) {
			pflt->pflt_skips++;
			goto out;
		}
		pflt->pflt_skips = 0;
	}

	/* take action now */
	faulted = 1;
	if (rcp) {
		if (pflt->pflt_retval)
			*rcp = pflt->pflt_retval;
		else
			*rcp = rc;
	}
	delay = pflt->pflt_delay;

	pflt->pflt_hits++;
	pfl_fault_unlock(pflt);

	if (delay)
		sleep(delay);
 out:
	if (!faulted) {
		pflt->pflt_unhits++;
		pfl_fault_unlock(pflt);
	}
	return (faulted);
}

/*
 * Send a response to a "getfault" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 */
int
psc_ctlrep_getfault(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	struct psc_ctlmsg_fault *pcflt = msg;
	struct pfl_fault *pflt;
	char name[PFL_FAULT_NAME_MAX];
	int i, rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pcflt->pcflt_name, sizeof(name));
	all = (name[0] == '\0');
	spinlock(&pfl_faults_lock);
	DYNARRAY_FOREACH(pflt, i, &pfl_faults)
		if (all || strncmp(pflt->pflt_name, name,
		    strlen(name)) == 0) {
			found = 1;

			pfl_fault_lock(pflt);
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
			pfl_fault_unlock(pflt);

			rc = psc_ctlmsg_sendv(fd, mh, pcflt, NULL);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(pflt->pflt_name, name) == 0)
				break;
		}
	freelock(&pfl_faults_lock);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, NULL, "unknown fault point: %s",
		    name);
	return (rc);
}

int
psc_ctlparam_faults_handle(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    struct pfl_fault *pflt, int val)
{
	char nbuf[20];
	int set;

	levels[1] = pflt->pflt_name;

	set = (mh->mh_type == PCMT_SETPARAM);

	if (nlevels < 3 || strcmp(levels[2], "active") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
				return (psc_ctlsenderr(fd, mh, NULL,
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
			/* reset counter */
			pflt->pflt_hits = 0;
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
	if (nlevels < 3 || strcmp(levels[2], "interval") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				pflt->pflt_interval += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				pflt->pflt_interval -= val;
			else
				pflt->pflt_interval = val;
		} else {
			levels[2] = "interval";
			snprintf(nbuf, sizeof(nbuf), "%d",
			    pflt->pflt_interval);
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
	struct pfl_fault *pflt;
	int i, rc, set;
	char *endp;
	long val;

	if (nlevels > 3)
		return (psc_ctlsenderr(fd, mh, NULL, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, NULL,"invalid thread field"));

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
	    strcmp(levels[2], "interval") != 0 &&
	    strcmp(levels[2], "retval") != 0)
		return (psc_ctlsenderr(fd, mh, NULL,
		    "invalid faults field: %s", levels[2]));

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (nlevels != 3)
			return (psc_ctlsenderr(fd, mh, NULL,
			    "invalid operation"));

		endp = NULL;
		val = strtol(pcp->pcp_value, &endp, 10);
		if (val == LONG_MIN || val == LONG_MAX ||
		    val > INT_MAX || val < 0 ||
		    endp == pcp->pcp_value || *endp != '\0')
			return (psc_ctlsenderr(fd, mh, NULL,
			    "invalid fault point %s value: %s",
			    levels[2], pcp->pcp_value));
	}

	if (nlevels == 1) {
		DYNARRAY_FOREACH(pflt, i, &pfl_faults) {
			pfl_fault_lock(pflt);
			rc = psc_ctlparam_faults_handle(fd, mh, pcp,
			    levels, nlevels, pflt, val);
			pfl_fault_unlock(pflt);
			if (!rc)
				break;
		}
	} else {
		if (set)
			pflt = pfl_fault_get(levels[1]);
		else
			pflt = pfl_fault_peek(levels[1]);
		if (pflt == NULL)
			return (psc_ctlsenderr(fd, mh, NULL,
			    "unknown fault point: %s", levels[1]));
		pfl_fault_lock(pflt);
		rc = psc_ctlparam_faults_handle(fd, mh, pcp, levels,
		    nlevels, pflt, val);
		pfl_fault_unlock(pflt);
	}
	return (rc);
}
