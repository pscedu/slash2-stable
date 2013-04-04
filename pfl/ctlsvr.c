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

/*
 * Control interface for querying and modifying parameters of a running
 * daemon instance.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/resource.h>

#include <errno.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/explist.h"
#include "pfl/hashtbl.h"
#include "pfl/pfl.h"
#include "pfl/rlimit.h"
#include "pfl/str.h"
#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_ds/stree.h"
#include "psc_rpc/rpc.h"
#include "psc_util/atomic.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlsvr.h"
#include "psc_util/fault.h"
#include "psc_util/fmtstr.h"
#include "psc_util/iostats.h"
#include "psc_util/journal.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"
#include "psc_util/mlist.h"
#include "psc_util/net.h"
#include "psc_util/pool.h"
#include "psc_util/random.h"
#include "psc_util/thread.h"
#include "psc_util/umask.h"
#include "psc_util/waitq.h"

#define QLEN 15	/* listen(2) queue */

struct psc_dynarray	psc_ctl_clifds = DYNARRAY_INIT;
psc_spinlock_t		psc_ctl_clifds_lock = SPINLOCK_INIT;
struct psc_waitq	psc_ctl_clifds_waitq = PSC_WAITQ_INIT;

struct pfl_mutex	pfl_ctl_mutex = PSC_MUTEX_INIT;

struct pfl_opstat	pflctl_opstats[OPSTATS_MAX];

__weak size_t
psc_multiwaitcond_nwaiters(__unusedx struct psc_multiwaitcond *m)
{
	psc_fatalx("multiwait support not compiled in");
}

__weak int
psc_ctlrep_getfault(int fd, struct psc_ctlmsghdr *mh,
    __unusedx void *c)
{
	return (psc_ctlsenderr(fd, mh, "fault support not compiled in"));
}

/**
 * psc_ctlmsg_sendv - Send a control message back to client.
 * @fd: client socket descriptor.
 * @mh: already filled-out control message header.
 * @m: control message contents.
 */
int
psc_ctlmsg_sendv(int fd, const struct psc_ctlmsghdr *mh, const void *m)
{
	struct iovec iov[2];
	struct msghdr msg;
	size_t tsiz;
	ssize_t n;

	iov[0].iov_base = (void *)mh;
	iov[0].iov_len = sizeof(*mh);

	iov[1].iov_base = (void *)m;
	iov[1].iov_len = mh->mh_size;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = nitems(iov);

	psc_mutex_lock(&pfl_ctl_mutex);
	n = sendmsg(fd, &msg, PFL_MSG_NOSIGNAL);
	psc_mutex_unlock(&pfl_ctl_mutex);

	if (n == -1) {
		if (errno == EPIPE || errno == ECONNRESET) {
			psc_ctlthr(pscthr_get())->pct_stat.ndrop++;
			sched_yield();
			return (0);
		}
		psc_fatal("sendmsg");
	}
	tsiz = sizeof(*mh) + mh->mh_size;
	if ((size_t)n != tsiz)
		psclog_warn("short sendmsg");
	psc_ctlthr(pscthr_get())->pct_stat.nsent++;
	sched_yield();
	return (1);
}

/**
 * psc_ctlmsg_send - Send a control message back to client.
 * @fd: client socket descriptor.
 * @id: client-provided passback identifier.
 * @type: type of message.
 * @siz: size of message.
 * @m: control message contents.
 * Notes: a control message header will be constructed and
 *	written to the client preceding the message contents.
 */
int
psc_ctlmsg_send(int fd, int id, int type, size_t siz, const void *m)
{
	struct psc_ctlmsghdr mh;

	memset(&mh, 0, sizeof(mh));
	mh.mh_id = id;
	mh.mh_type = type;
	mh.mh_size = siz;
	return (psc_ctlmsg_sendv(fd, &mh, m));
}

int
psc_ctlsenderrv(int fd, const struct psc_ctlmsghdr *mhp, const char *fmt,
    va_list ap)
{
	struct psc_ctlmsg_error pce;
	struct psc_ctlmsghdr mh;

	vsnprintf(pce.pce_errmsg, sizeof(pce.pce_errmsg), fmt, ap); /* XXX */

	mh.mh_id = mhp->mh_id;
	mh.mh_type = PCMT_ERROR;
	mh.mh_size = sizeof(pce);
	return (psc_ctlmsg_sendv(fd, &mh, &pce));
}

/**
 * psc_ctlsenderr - Send an error to client over control interface.
 * @fd: client socket descriptor.
 * @mh: message header to use.
 * @fmt: printf(3) format of error message.
 */
int
psc_ctlsenderr(int fd, const struct psc_ctlmsghdr *mhp, const char *fmt,
    ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = psc_ctlsenderrv(fd, mhp, fmt, ap);
	va_end(ap);
	return (rc);
}

/**
 * psc_ctlthr_get - Export control thread stats.
 * @thr: thread begin queried.
 * @pcst: thread stats control message to be filled in.
 */
void
psc_ctlthr_get(struct psc_thread *thr, struct psc_ctlmsg_thread *pcst)
{
	pcst->pcst_nsent	= psc_ctlthr(thr)->pct_stat.nsent;
	pcst->pcst_nrecv	= psc_ctlthr(thr)->pct_stat.nrecv;
	pcst->pcst_ndrop	= psc_ctlthr(thr)->pct_stat.ndrop;
}

/**
 * psc_ctlacthr_get - Export control thread stats.
 * @thr: thread begin queried.
 * @pcst: thread stats control message to be filled in.
 */
void
psc_ctlacthr_get(struct psc_thread *thr, struct psc_ctlmsg_thread *pcst)
{
	pcst->pcst_nclients	= psc_ctlacthr(thr)->pcat_stat.nclients;
}

/**
 * psc_ctlmsg_thread_send - Send a reply to a "GETTHREAD" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 * @thr: thread begin queried.
 */
__static int
psc_ctlmsg_thread_send(int fd, struct psc_ctlmsghdr *mh, void *m,
    struct psc_thread *thr)
{
	struct psc_ctlmsg_thread *pcst = m;

	snprintf(pcst->pcst_thrname, sizeof(pcst->pcst_thrname),
	    "%s", thr->pscthr_name);
	pcst->pcst_thrid = thr->pscthr_thrid;
	pcst->pcst_thrtype = thr->pscthr_type;
	pcst->pcst_flags = thr->pscthr_flags;
	if (thr->pscthr_type >= 0 &&
	    thr->pscthr_type < psc_ctl_nthrgets &&
	    psc_ctl_thrgets[thr->pscthr_type])
		psc_ctl_thrgets[thr->pscthr_type](thr, pcst);
	return (psc_ctlmsg_sendv(fd, mh, pcst));
}

/**
 * psc_ctlrep_getthread - Respond to a "GETTHREAD" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine and reuse.
 */
int
psc_ctlrep_getthread(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_thread *pcst = m;

	return (psc_ctl_applythrop(fd, mh, m, pcst->pcst_thrname,
	    psc_ctlmsg_thread_send));
}

/**
 * psc_ctlrep_getsubsys - Send a response to a "GETSUBSYS" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 */
int
psc_ctlrep_getsubsys(int fd, struct psc_ctlmsghdr *mh, __unusedx void *m)
{
	struct psc_ctlmsg_subsys *pcss;
	size_t siz;
	int n, rc;

	rc = 1;
	siz = PCSS_NAME_MAX * psc_nsubsys;
	pcss = PSCALLOC(siz);
	for (n = 0; n < psc_nsubsys; n++)
		if (snprintf(&pcss->pcss_names[n * PCSS_NAME_MAX],
		    PCSS_NAME_MAX, "%s", psc_subsys_name(n)) == -1) {
			psclog_warn("snprintf");
			rc = psc_ctlsenderr(fd, mh,
			    "unable to retrieve subsystems");
			goto done;
		}
	mh->mh_size = siz;
	rc = psc_ctlmsg_sendv(fd, mh, pcss);
 done:
	mh->mh_size = 0;	/* reset because we used our own buffer */
	PSCFREE(pcss);
	return (rc);
}

__weak int
psc_ctlrep_getlni(int fd, struct psc_ctlmsghdr *mh,
    __unusedx void *m)
{
	return (psc_ctlsenderr(fd, mh, "get lnet interface: %s",
	    strerror(ENOTSUP)));
}

/**
 * psc_ctlmsg_loglevel_send - Send a reply to a "GETLOGLEVEL" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @thr: thread begin queried.
 */
__static int
psc_ctlmsg_loglevel_send(int fd, struct psc_ctlmsghdr *mh, void *m,
    struct psc_thread *thr)
{
	struct psc_ctlmsg_loglevel *pcl = m;
	size_t siz;
	int rc;

	siz = sizeof(*pcl) + sizeof(*pcl->pcl_levels) * psc_nsubsys;
	pcl = PSCALLOC(siz);
	snprintf(pcl->pcl_thrname, sizeof(pcl->pcl_thrname),
	    "%s", thr->pscthr_name);
	memcpy(pcl->pcl_levels, thr->pscthr_loglevels, psc_nsubsys *
	    sizeof(*pcl->pcl_levels));
	mh->mh_size = siz;
	rc = psc_ctlmsg_sendv(fd, mh, pcl);
	mh->mh_size = 0;	/* reset because we used our own buffer */
	PSCFREE(pcl);
	return (rc);
}

/**
 * psc_ctlrep_getloglevel - Respond to a "GETLOGLEVEL" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine.
 */
int
psc_ctlrep_getloglevel(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_loglevel *pcl = m;

	return (psc_ctl_applythrop(fd, mh, m, pcl->pcl_thrname,
	    psc_ctlmsg_loglevel_send));
}

/**
 * psc_ctlrep_gethashtable - Respond to a "GETHASHTABLE" inquiry.
 *	This computes bucket usage statistics of a hash table and
 *	sends the results back to the client.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_gethashtable(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_hashtable *pcht = m;
	struct psc_hashtbl *pht;
	char name[PSC_HTNAME_MAX];
	int rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pcht->pcht_name, sizeof(name));
	all = (name[0] == '\0');

	PLL_LOCK(&psc_hashtbls);
	PLL_FOREACH(pht, &psc_hashtbls) {
		if (all ||
		    strncmp(name, pht->pht_name, strlen(name)) == 0) {
			found = 1;

			snprintf(pcht->pcht_name, sizeof(pcht->pcht_name),
			    "%s", pht->pht_name);
			psc_hashtbl_getstats(pht, &pcht->pcht_totalbucks,
			    &pcht->pcht_usedbucks, &pcht->pcht_nents,
			    &pcht->pcht_maxbucklen);
			rc = psc_ctlmsg_sendv(fd, mh, pcht);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(pht->pht_name, name) == 0)
				break;
		}
	}
	PLL_ULOCK(&psc_hashtbls);

	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown hash table: %s",
		    name);
	return (rc);
}

/**
 * psc_ctlrep_getlc - Respond to a "GETLC" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine and reuse.
 */
int
psc_ctlrep_getlc(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_lc *pclc = m;
	struct psc_listcache *lc;
	char name[PEXL_NAME_MAX];
	int rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pclc->pclc_name, sizeof(name));
	all = (strcmp(name, PCLC_NAME_ALL) == 0 || name[0] == '\0');
	PLL_LOCK(&psc_listcaches);
	PLL_FOREACH(lc, &psc_listcaches) {
		if (all || strncmp(lc->plc_name,
		    name, strlen(name)) == 0) {
			found = 1;

			LIST_CACHE_LOCK(lc);
			strlcpy(pclc->pclc_name, lc->plc_name,
			    sizeof(pclc->pclc_name));
			pclc->pclc_size = lc->plc_nitems;
			pclc->pclc_nseen = lc->plc_nseen;
			pclc->pclc_flags = lc->plc_flags;
			pclc->pclc_nw_want = psc_waitq_nwaiters(
			    &lc->plc_wq_want);
			pclc->pclc_nw_empty = psc_waitq_nwaiters(
			    &lc->plc_wq_empty);
			LIST_CACHE_ULOCK(lc);
			rc = psc_ctlmsg_sendv(fd, mh, pclc);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(lc->plc_name, name) == 0)
				break;
		}
	}
	PLL_ULOCK(&psc_listcaches);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown listcache: %s",
		    name);
	return (rc);
}

/**
 * psc_ctlrep_getpool - Send a response to a "GETPOOL" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @msg: control message to examine and reuse.
 */
int
psc_ctlrep_getpool(int fd, struct psc_ctlmsghdr *mh, void *msg)
{
	struct psc_ctlmsg_pool *pcpl = msg;
	struct psc_poolmgr *m;
	char name[PEXL_NAME_MAX];
	int rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pcpl->pcpl_name, sizeof(name));
	all = (strcmp(name, PCPL_NAME_ALL) == 0 || name[0] == '\0');
	PLL_LOCK(&psc_pools);
	PLL_FOREACH(m, &psc_pools) {
		if (all || strncmp(m->ppm_name, name,
		    strlen(name)) == 0) {
			found = 1;

			POOL_LOCK(m);
			strlcpy(pcpl->pcpl_name, m->ppm_name,
			    sizeof(pcpl->pcpl_name));
			pcpl->pcpl_min = m->ppm_min;
			pcpl->pcpl_max = m->ppm_max;
			pcpl->pcpl_total = m->ppm_total;
			pcpl->pcpl_flags = m->ppm_flags;
			pcpl->pcpl_thres = m->ppm_thres;
			pcpl->pcpl_nseen = m->ppm_nseen;
			pcpl->pcpl_ngrow = m->ppm_ngrow;
			pcpl->pcpl_nshrink = m->ppm_nshrink;
			if (POOL_IS_MLIST(m)) {
				pcpl->pcpl_free = psc_mlist_size(&m->ppm_ml);
				pcpl->pcpl_nw_empty = psc_multiwaitcond_nwaiters(
				    &m->ppm_ml.pml_mwcond_empty);
			} else {
				pcpl->pcpl_free = lc_nitems(&m->ppm_lc);
				pcpl->pcpl_nw_want = psc_waitq_nwaiters(
				    &m->ppm_lc.plc_wq_want);
				pcpl->pcpl_nw_empty = psc_waitq_nwaiters(
				    &m->ppm_lc.plc_wq_empty);
			}
			POOL_ULOCK(m);
			rc = psc_ctlmsg_sendv(fd, mh, pcpl);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(m->ppm_name, name) == 0)
				break;
		}
	}
	PLL_ULOCK(&psc_pools);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown pool: %s", name);
	return (rc);
}

/* Maximum depth of parameter node, e.g. [thr.]foo.bar.glarch=3 */
#define MAX_LEVELS 8

int
psc_ctlmsg_param_send(int fd, const struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, const char *thrname,
    char **levels, int nlevels, const char *value)
{
	char *s, othrname[PSC_THRNAME_MAX];
	const char *p, *end;
	int rc, lvl;

	/*
	 * Save original request threadname and copy actual in
	 * for this message.  These will differ in cases such as
	 * "all" or "mythr" against "mythr9".
	 */
	snprintf(othrname, sizeof(othrname), "%s", pcp->pcp_thrname);
	snprintf(pcp->pcp_thrname, sizeof(pcp->pcp_thrname), "%s",
	    thrname);

	/* Concatenate each levels[] element together with dots (`.'). */
	s = pcp->pcp_field;
	end = s + sizeof(pcp->pcp_field) - 1;
	for (lvl = 0; s < end && lvl < nlevels; lvl++) {
		for (p = levels[lvl]; s < end && *p; s++, p++)
			*s = *p;
		if (s < end && lvl < nlevels - 1)
			*s++ = '.';
	}
	*s = '\0';

	snprintf(pcp->pcp_value, sizeof(pcp->pcp_value), "%s", value);
	rc = psc_ctlmsg_sendv(fd, mh, pcp);

	/* Restore original threadname value for additional processing. */
	snprintf(pcp->pcp_thrname, sizeof(pcp->pcp_thrname), "%s",
	    othrname);
	return (rc);
}

int
psc_ctlparam_log_level(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int found, rc, set, loglevel, subsys, start_ss, end_ss;
	struct psc_thread *thr;

	if (nlevels > 3)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	levels[0] = "log";
	levels[1] = "level";

	loglevel = 0; /* gcc */

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		loglevel = psc_loglevel_fromstr(pcp->pcp_value);
		if (loglevel == PNLOGLEVELS)
			return (psc_ctlsenderr(fd, mh,
			    "invalid log.level value: %s",
			    pcp->pcp_value));
		if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));
	}

	if (nlevels == 3) {
		/* Subsys specified, use it. */
		subsys = psc_subsys_id(levels[2]);
		if (subsys == -1)
			return (psc_ctlsenderr(fd, mh,
			    "invalid log.level subsystem: %s",
			    levels[2]));
		start_ss = subsys;
		end_ss = subsys + 1;
	} else {
		/* No subsys specified, use all. */
		start_ss = 0;
		end_ss = psc_nsubsys;
		subsys = PSS_ALL;
	}

	if (set && strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) == 0)
		psc_log_setlevel(subsys, loglevel);
		/* XXX optimize: bail */

	rc = 1;
	found = 0;
	PLL_LOCK(&psc_threads);
	PSC_CTL_FOREACH_THREAD(thr, pcp->pcp_thrname,
	    &psc_threads.pll_listhd) {
		found = 1;

		for (subsys = start_ss; subsys < end_ss; subsys++) {
			if (set)
				thr->pscthr_loglevels[subsys] = loglevel;
			else {
				levels[2] = (char *)psc_subsys_name(subsys);
				rc = psc_ctlmsg_param_send(fd, mh, pcp,
				    thr->pscthr_name, levels, 3,
				    psc_loglevel_getname(thr->
				    pscthr_loglevels[subsys]));
				if (!rc)
					goto done;
			}
		}
	}

 done:
	PLL_ULOCK(&psc_threads);
	if (!found)
		return (psc_ctlsenderr(fd, mh, "invalid thread: %s",
		    pcp->pcp_thrname));
	return (rc);
}

int
psc_ctlparam_log_file(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int rc, set;

	if (nlevels > 2)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	rc = 1;
	levels[0] = "log";
	levels[1] = "file";

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (pcp->pcp_flags & PCPF_SUB)
			return (psc_ctlsenderr(fd,
			    mh, "invalid operation"));
		if (freopen(pcp->pcp_value,
		    pcp->pcp_flags & PCPF_ADD ?
		    "a" : "w", stderr) == NULL)
			rc = psc_ctlsenderr(fd, mh, "log.file: %s",
			    strerror(errno));
	} else
		rc = psc_ctlsenderr(fd, mh, "log.file: write-only");
	return (rc);
}

int
psc_ctlparam_log_format(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int rc, set;

	if (nlevels > 2)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	rc = 1;
	levels[0] = "log";
	levels[1] = "format";

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		static char logbuf[LINE_MAX];

		if (nlevels != 2)
			return (psc_ctlsenderr(fd, mh,
			    "invalid thread field"));

		if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));

		strlcpy(logbuf, pcp->pcp_value, sizeof(logbuf));
		psc_logfmt = logbuf;
	} else
		rc = psc_ctlmsg_param_send(fd, mh, pcp,
		    PCTHRNAME_EVERYONE, levels, 2, psc_logfmt);
	return (rc);
}

struct psc_ctl_rlim {
	char	*pcr_name;
	int	 pcr_id;
} psc_ctl_rlimtab [] = {
	{ "cpu",	RLIMIT_CPU },
	{ "csize",	RLIMIT_CORE },
	{ "dsize",	RLIMIT_DATA },
	{ "fsize",	RLIMIT_FSIZE },
#ifdef RLIMIT_NPROC
	{ "maxproc",	RLIMIT_NPROC },
#endif
	{ "mem",	RLIMIT_AS },
#ifdef RLIMIT_MEMLOCK
	{ "mlock",	RLIMIT_MEMLOCK },
#endif
	{ "nofile",	RLIMIT_NOFILE },
	{ "stksize",	RLIMIT_STACK }
};

int
psc_ctlparam_rlim(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	struct psc_ctl_rlim *pcr = NULL;
	char buf[32], *endp;
	int rc, set, i;
	long val = 0;
	rlim_t n;

	if (nlevels > 2)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	rc = 1;
	levels[0] = "rlim";

	if (nlevels == 2) {
		for (pcr = psc_ctl_rlimtab, i = 0;
		    i < (int)nitems(psc_ctl_rlimtab); i++, pcr++)
			if (strcmp(levels[1], pcr->pcr_name) == 0)
				break;
		if (i == nitems(psc_ctl_rlimtab))
			return (psc_ctlsenderr(fd, mh,
			    "invalid rlim field: %s", levels[1]));
	}

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (nlevels != 2)
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));
		endp = NULL;
		val = strtol(pcp->pcp_value, &endp, 10);
		if (val <= 0 || val > 10 * 1024 ||
		    endp == pcp->pcp_value || *endp != '\0')
			return (psc_ctlsenderr(fd, mh,
			    "invalid rlim.%s value: %s",
			    pcr->pcr_name, pcp->pcp_value));

		if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB)) {
			if (psc_getrlimit(pcr->pcr_id, &n, NULL) == -1) {
				psclog_error("getrlimit");
				return (psc_ctlsenderr(fd, mh,
				    "getrlimit", strerror(errno)));
			}
			if (pcp->pcp_flags & PCPF_ADD)
				val += n;
			else if (pcp->pcp_flags & PCPF_SUB)
				val = n - val;
		}
	}

	for (pcr = psc_ctl_rlimtab, i = 0;
	    i < (int)nitems(psc_ctl_rlimtab); i++, pcr++) {
		if (nlevels < 2 ||
		    strcmp(levels[1], pcr->pcr_name) == 0) {
			if (set) {
				if (psc_setrlimit(pcr->pcr_id, val,
				    val) == -1)
					return (psc_ctlsenderr(fd, mh,
					    "setrlimit", strerror(errno)));
			} else {
				levels[1] = pcr->pcr_name;
				if (psc_getrlimit(pcr->pcr_id, &n,
				    NULL) == -1) {
					psclog_error("getrlimit");
					return (psc_ctlsenderr(fd, mh,
					    "getrlimit", strerror(errno)));
				}
				snprintf(buf, sizeof(buf), "%"PRId64, n);
				rc = psc_ctlmsg_param_send(fd, mh, pcp,
				    PCTHRNAME_EVERYONE, levels, 2, buf);
			}
			if (nlevels == 2)
				break;
		}
	}
	return (rc);
}

int
psc_ctlparam_run(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int found, rc, set, run;
	struct psc_thread *thr;

	if (nlevels > 1)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	levels[0] = "run";
	run = 0; /* gcc */

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));

		if (strcmp(pcp->pcp_value, "0") == 0)
			run = 0;
		else if (strcmp(pcp->pcp_value, "1") == 0)
			run = 1;
		else
			return (psc_ctlsenderr(fd, mh,
			    "invalid run value: %s",
			    pcp->pcp_field));
	}

	rc = 1;
	found = 0;
	PLL_LOCK(&psc_threads);
	PSC_CTL_FOREACH_THREAD(thr, pcp->pcp_thrname,
	    &psc_threads.pll_listhd) {
		found = 1;

		if (set)
			pscthr_setrun(thr, run);
		else if (!(rc = psc_ctlmsg_param_send(fd, mh, pcp,
		    thr->pscthr_name, levels, 1,
		    thr->pscthr_flags & PTF_RUN ? "1" : "0")))
			break;
	}
	PLL_ULOCK(&psc_threads);
	if (!found)
		return (psc_ctlsenderr(fd, mh, "invalid thread: %s",
		    pcp->pcp_thrname));
	return (rc);
}

/**
 * psc_ctlparam_pause - Handle thread pause state parameter.
 * @fd: control connection file descriptor.
 * @mh: already filled-in control message header.
 * @pcp: parameter control message.
 * @levels: parameter fields.
 * @nlevels: number of fields.
 */
int
psc_ctlparam_pause(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int found, rc, set, pauseval;
	struct psc_thread *thr;
	char *s;
	long l;

	if (nlevels > 1)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	levels[0] = "pause";

	pauseval = 0; /* gcc */

	set = (mh->mh_type == PCMT_SETPARAM);

	if (set) {
		if (pcp->pcp_flags & (PCPF_ADD | PCPF_SUB))
			return (psc_ctlsenderr(fd, mh,
			    "invalid operation"));

		s = NULL;
		l = strtol(pcp->pcp_value, &s, 10);
		if (l == LONG_MAX || l == LONG_MIN || *s != '\0' ||
		    s == pcp->pcp_value || l < 0 || l > 1)
			return (psc_ctlsenderr(fd, mh,
			    "invalid pause value: %s",
			    pcp->pcp_field));
		pauseval = (int)l;
	}

	rc = 1;
	found = 0;
	PLL_LOCK(&psc_threads);
	PSC_CTL_FOREACH_THREAD(thr, pcp->pcp_thrname,
	    &psc_threads.pll_listhd) {
		found = 1;

		if (set)
			pscthr_setpause(thr, pauseval);
		else if (!(rc = psc_ctlmsg_param_send(fd, mh, pcp,
		    thr->pscthr_name, levels, 1,
		    (thr->pscthr_flags & PTF_PAUSED) ? "1" : "0")))
			break;
	}
	PLL_ULOCK(&psc_threads);
	if (!found)
		return (psc_ctlsenderr(fd, mh, "invalid thread: %s",
		    pcp->pcp_thrname));
	return (rc);
}

int
psc_ctlparam_pool_handle(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    struct psc_poolmgr *m, int val)
{
	char nbuf[20];
	int set;

	levels[0] = "pool";
	levels[1] = m->ppm_name;

	set = (mh->mh_type == PCMT_SETPARAM);

	if (nlevels < 3 || strcmp(levels[2], "min") == 0) {
		if (nlevels == 3 && set) {
			/* XXX logic is bogus */
			if (pcp->pcp_flags & PCPF_ADD)
				m->ppm_min += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				m->ppm_min -= val;
			else
				m->ppm_min = val;
			if (m->ppm_min < 1)
				m->ppm_min = 1;
			if (m->ppm_min > m->ppm_max)
				m->ppm_min = m->ppm_max;
			psc_pool_resize(m);
		} else {
			levels[2] = "min";
			snprintf(nbuf, sizeof(nbuf), "%d", m->ppm_min);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "max") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				m->ppm_max += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				m->ppm_max -= val;
			else
				m->ppm_max = val;
			if (m->ppm_max < 1)
				m->ppm_max = 1;
			if (m->ppm_max < m->ppm_min)
				m->ppm_max = m->ppm_min;
			psc_pool_resize(m);
		} else {
			levels[2] = "max";
			snprintf(nbuf, sizeof(nbuf), "%d", m->ppm_max);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "total") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				psc_pool_grow(m, val);
			else if (pcp->pcp_flags & PCPF_SUB)
				psc_pool_tryshrink(m, val);
			else
				psc_pool_settotal(m, val);
		} else {
			levels[2] = "total";
			snprintf(nbuf, sizeof(nbuf), "%d", m->ppm_total);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	if (nlevels < 3 || strcmp(levels[2], "thres") == 0) {
		if (nlevels == 3 && set) {
			if (pcp->pcp_flags & PCPF_ADD)
				m->ppm_thres += val;
			else if (pcp->pcp_flags & PCPF_SUB)
				m->ppm_thres -= val;
			else
				m->ppm_thres = val;
			if (m->ppm_thres < 1)
				m->ppm_thres = 1;
			else if (m->ppm_thres > 99)
				m->ppm_thres = 99;
		} else {
			levels[2] = "thres";
			snprintf(nbuf, sizeof(nbuf), "%d", m->ppm_thres);
			if (!psc_ctlmsg_param_send(fd, mh, pcp,
			    PCTHRNAME_EVERYONE, levels, 3, nbuf))
				return (0);
		}
	}
	return (1);
}

int
psc_ctlparam_pool(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	struct psc_poolmgr *m;
	int rc, set;
	char *endp;
	long val;

	if (nlevels > 3)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid field for %s",
		    pcp->pcp_thrname));

	rc = 1;
	levels[0] = "pool";
	val = 0; /* gcc */

	if (nlevels == 3 &&
	    strcmp(levels[2], "min")   != 0 &&
	    strcmp(levels[2], "max")   != 0 &&
	    strcmp(levels[2], "thres") != 0 &&
	    strcmp(levels[2], "total") != 0)
		return (psc_ctlsenderr(fd, mh,
		    "invalid pool field: %s", levels[2]));

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
			    "invalid pool %s value: %s",
			    levels[2], pcp->pcp_value));
	}

	if (nlevels == 1) {
		PLL_LOCK(&psc_pools);
		PLL_FOREACH(m, &psc_pools) {
			POOL_LOCK(m);
			rc = psc_ctlparam_pool_handle(fd, mh, pcp,
			    levels, nlevels, m, val);
			POOL_ULOCK(m);
			if (!rc)
				break;
		}
		PLL_ULOCK(&psc_pools);
	} else {
		m = psc_pool_lookup(levels[1]);
		if (m == NULL)
			return (psc_ctlsenderr(fd, mh,
			    "invalid pool: %s", levels[1]));
		rc = psc_ctlparam_pool_handle(fd, mh, pcp, levels,
		    nlevels, m, val);
		POOL_ULOCK(m);
	}
	return (rc);
}

/* Node in the control parameter tree. */
struct psc_ctlparam_node {
	char			 *pcn_name;
	int			(*pcn_cbf)(int, struct psc_ctlmsghdr *,
				    struct psc_ctlmsg_param *, char **,
				    int, struct psc_ctlparam_node *);

	/* only used for SIMPLE ctlparam nodes */
	void			(*pcn_getf)(char [PCP_VALUE_MAX]);
	int			(*pcn_setf)(const char *);
};

/* Stack processing frame. */
struct psc_ctlparam_procframe {
	struct psc_listentry	 pcf_lentry;
	struct psc_streenode	*pcf_ptn;
	int			 pcf_level;
	int			 pcf_flags;
	int			 pcf_pos;
};

/* pcf_flags */
#define PCFF_USEPOS		(1 << 0)

struct psc_streenode psc_ctlparamtree = PSC_STREE_INIT(psc_ctlparamtree);

const char *
psc_ctlparam_fieldname(char *fieldname, int nlevels)
{
	while (nlevels-- > 1)
		fieldname[strlen(fieldname)] = '.';
	return (fieldname);
}

int
psc_ctlrep_param_simple(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    struct psc_ctlparam_node *pcn)
{
	char val[PCP_VALUE_MAX];

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) != 0)
		return (psc_ctlsenderr(fd, mh, "invalid thread field"));

	if (mh->mh_type == PCMT_SETPARAM) {
		if (pcn->pcn_setf) {
			if (pcn->pcn_setf(pcp->pcp_value))
				return (psc_ctlsenderr(fd, mh,
				    "%s: invalid value: %s",
				    psc_ctlparam_fieldname(
				      pcp->pcp_field, nlevels),
				    pcp->pcp_value));
			return (1);
		}
		return (psc_ctlsenderr(fd, mh, "%s: field is read-only",
		    psc_ctlparam_fieldname(pcp->pcp_field, nlevels)));
	}
	pcn->pcn_getf(val);
	return (psc_ctlmsg_param_send(fd, mh, pcp,
	    PCTHRNAME_EVERYONE, levels, nlevels, val));
}

int
psc_ctlrep_param(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlparam_procframe *pcf, *npcf;
	struct psc_streenode *ptn, *c, *d;
	struct psc_ctlmsg_param *pcp = m;
	struct psc_ctlparam_node *pcn;
	struct psclist_head stack;
	char *t, *levels[MAX_LEVELS];
	int n, k, nlevels, set, rc = 1;
	uid_t uid;
	gid_t gid;

	pcf = NULL;
	INIT_PSCLIST_HEAD(&stack);

	pcp->pcp_thrname[sizeof(pcp->pcp_thrname) - 1] = '\0';
	pcp->pcp_field[sizeof(pcp->pcp_field) - 1] = '\0';
	pcp->pcp_value[sizeof(pcp->pcp_value) - 1] = '\0';

	set = (mh->mh_type == PCMT_SETPARAM);

	for (nlevels = 0, t = pcp->pcp_field;
	    nlevels < MAX_LEVELS &&
	    (levels[nlevels] = t) != NULL; ) {
		if ((t = strchr(levels[nlevels], '.')) != NULL)
			*t++ = '\0';
		if (*levels[nlevels++] == '\0')
			return (psc_ctlsenderr(fd, mh,
			    "%s: empty node name",
			    psc_ctlparam_fieldname(pcp->pcp_field,
			    nlevels)));
	}

	if (nlevels == 0)
		return (psc_ctlsenderr(fd, mh,
		    "no parameter field specified"));
	if (nlevels >= MAX_LEVELS)
		return (psc_ctlsenderr(fd, mh,
		    "%s: parameter field exceeds maximum depth",
		    psc_ctlparam_fieldname(pcp->pcp_field,
		    MAX_LEVELS)));

	if (set) {
		rc = pfl_socket_getpeercred(fd, &uid, &gid);
		if (rc == 0 && uid)
			rc = EPERM;
		if (rc)
			return (psc_ctlsenderr(fd, mh, "%s: %s",
			    psc_ctlparam_fieldname(pcp->pcp_field,
			      nlevels), strerror(rc)));
	}

	pcf = PSCALLOC(sizeof(*pcf));
	INIT_PSC_LISTENTRY(&pcf->pcf_lentry);
	pcf->pcf_ptn = &psc_ctlparamtree;
	psclist_add(&pcf->pcf_lentry, &stack);

	while (!psc_listhd_empty(&stack)) {
		pcf = psc_listhd_first_obj(&stack,
		    struct psc_ctlparam_procframe, pcf_lentry);
		psclist_del(&pcf->pcf_lentry, &stack);

		n = pcf->pcf_level;
		ptn = pcf->pcf_ptn;
		do {
			if (n == nlevels - 1 && strcmp(levels[n], "?") == 0) {
				if (n)
					psc_ctlparam_fieldname(pcp->pcp_value,
					    nlevels - 1);
				PSC_STREE_FOREACH_CHILD(c, ptn) {
					pcn = c->ptn_data;
					if (n)
						rc = psc_ctlsenderr(fd, mh,
						    "%s: available subnode: %s",
						    pcp->pcp_field, pcn->pcn_name);
					else
						rc = psc_ctlsenderr(fd, mh,
						    "available top-level node: %s",
						    pcn->pcn_name);
					if (rc == 0)
						break;
				}
				goto shortcircuit;
			}

			k = 0;
			PSC_STREE_FOREACH_CHILD(c, ptn) {
				pcn = c->ptn_data;
				if (pcf->pcf_flags & PCFF_USEPOS) {
					if (pcf->pcf_pos == k)
						break;
				} else if (n < nlevels &&
				    strcmp(pcn->pcn_name,
				    levels[n]) == 0)
					break;
				k++;
			}
			if (c == NULL)
				goto invalid;

			levels[n] = pcn->pcn_name;

			if (psc_listhd_empty(&c->ptn_children)) {
				rc = pcn->pcn_cbf(fd, mh, pcp, levels,
				    nlevels, pcn);
				if (rc == 0)
					goto shortcircuit;
				break;
			} else if (pcf->pcf_level + 1 >= nlevels) {
				/* disallow setting values of non-leaf nodes */
				if (set)
					goto invalid;

				k = 0;
				PSC_STREE_FOREACH_CHILD(d, c) {
					pcn = d->ptn_data;
					/* avoid stack frame by processing directly */
					if (psc_listhd_empty(&d->ptn_children)) {
						levels[n + 1] = pcn->pcn_name;
						rc = pcn->pcn_cbf(fd,
						    mh, pcp, levels,
						    n + 2, pcn);
						if (rc == 0)
							goto shortcircuit;
					} else {
						npcf = PSCALLOC(sizeof(*npcf));
						npcf->pcf_ptn = d;
						npcf->pcf_level = n + 1;
						npcf->pcf_pos = k++;
						npcf->pcf_flags = PCFF_USEPOS;
						psclist_add(&npcf->pcf_lentry,
						    &stack);
					}
				}
			}
			ptn = c;
		} while (++n < nlevels);
		PSCFREE(pcf);
	}
	return (1);

 shortcircuit:
	PSCFREE(pcf);
	psclist_for_each_entry_safe(pcf, npcf, &stack, pcf_lentry)
		PSCFREE(pcf);
	return (rc);

 invalid:
	PSCFREE(pcf);
	/*
	 * Strictly speaking, this shouldn't be necessary, cause
	 * any frames we added were done out of the integrity of
	 * the paramtree.
	 */
	psclist_for_each_entry_safe(pcf, npcf, &stack, pcf_lentry)
		PSCFREE(pcf);
	psc_ctlparam_fieldname(pcp->pcp_field, nlevels);
	if (set)
		return (psc_ctlsenderr(fd, mh,
		    "%s: not a leaf node", pcp->pcp_field));
	return (psc_ctlsenderr(fd, mh, "%s: invalid field", pcp->pcp_field));
}

struct psc_ctlparam_node *
psc_ctlparam_register(const char *oname, int (*cbf)(int,
    struct psc_ctlmsghdr *, struct psc_ctlmsg_param *, char **, int,
    struct psc_ctlparam_node *))
{
	struct psc_streenode *ptn, *c;
	struct psc_ctlparam_node *pcn;
	char *subname, *next, *name;

	pcn = NULL; /* gcc */
	name = pfl_strdup(oname);
	ptn = &psc_ctlparamtree;
	for (subname = name; subname != NULL; subname = next) {
		if ((next = strchr(subname, '.')) != NULL)
			*next++ = '\0';
		PSC_STREE_FOREACH_CHILD(c, ptn) {
			pcn = c->ptn_data;
			if (strcmp(pcn->pcn_name, subname) == 0)
				break;
		}
		if (c == NULL) {
			pcn = PSCALLOC(sizeof(*pcn));
			pcn->pcn_name = pfl_strdup(subname);
			if (next == NULL)
				pcn->pcn_cbf = cbf;
			c = psc_stree_addchild(ptn, pcn);
		}
		ptn = c;
	}
	PSCFREE(name);
	return (pcn);
}

void
psc_ctlparam_register_simple(const char *name, void (*getf)(char *),
    int (*setf)(const char *))
{
	struct psc_ctlparam_node *pcn;

	pcn = psc_ctlparam_register(name, psc_ctlrep_param_simple);
	pcn->pcn_getf = getf;
	pcn->pcn_setf = setf;
}

/**
 * psc_ctlparam_opstats - Handle opstats parameter.
 * @fd: control connection file descriptor.
 * @mh: already filled-in control message header.
 * @pcp: parameter control message.
 * @levels: parameter fields.
 * @nlevels: number of fields.
 */
int
psc_ctlparam_opstats(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp, char **levels, int nlevels,
    __unusedx struct psc_ctlparam_node *pcn)
{
	int reset = 0, found = 0, rc = 1, i;
	struct pfl_opstat *pos;
	char buf[32];
	long val;

	if (nlevels > 2)
		return (psc_ctlsenderr(fd, mh, "invalid field"));

	levels[0] = "opstats";

	reset = (mh->mh_type == PCMT_SETPARAM);

	for (i = 0; i < (int)nitems(pflctl_opstats); i++) {
		pos = &pflctl_opstats[i];
		if (pos->pos_name == NULL)
			continue;
		if (nlevels < 2 ||
		    strcmp(levels[1], pos->pos_name) == 0) {
			found = 1;

			if (reset) {
				errno = 0;
				val = strtol(pcp->pcp_value, NULL, 10);
				if (errno == ERANGE)
					return (psc_ctlsenderr(fd, mh,
					    "invalid opstat %s value: %s",
					    levels[1], pcp->pcp_value));
				psc_atomic64_set(&pos->pos_value, val);
			} else {
				levels[1] = (char *)pos->pos_name;
				snprintf(buf, sizeof(buf), "%"PRId64,
				    psc_atomic64_read(&pos->pos_value));
				rc = psc_ctlmsg_param_send(fd, mh, pcp,
				    PCTHRNAME_EVERYONE, levels, 2, buf);
			}

			if (nlevels == 2)
				break;
		}
	}
	if (!found && nlevels > 1)
		return (psc_ctlsenderr(fd, mh, "%s: invalid opstat",
		    psc_ctlparam_fieldname(pcp->pcp_field, nlevels)));
	return (rc);
}

/**
 * psc_ctlrep_getiostats - Respond to a "GETIOSTATS" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: iostats control message to be filled in and sent out.
 */
int
psc_ctlrep_getiostats(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_iostats *pci = m;
	char name[IST_NAME_MAX];
	struct psc_iostats *ist;
	int rc, found, all;

	rc = 1;
	found = 0;
	snprintf(name, sizeof(name), "%s", pci->pci_ist.ist_name);
	all = (strcmp(name, PCI_NAME_ALL) == 0 || strcmp(name, "") == 0);
	PLL_LOCK(&psc_iostats);
	psclist_for_each_entry(ist,
	    &psc_iostats.pll_listhd, ist_lentry)
		if (all ||
		    fnmatch(name, ist->ist_name, 0) == 0) {
			found = 1;

			pci->pci_ist = *ist; /* XXX lock? */
			rc = psc_ctlmsg_sendv(fd, mh, pci);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(ist->ist_name, name) == 0)
				break;
		}
	PLL_ULOCK(&psc_iostats);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown iostats: %s",
		    name);
	return (rc);
}

/**
 * psc_ctlrep_getmeter - Respond to a "GETMETER" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine and reuse.
 */
int
psc_ctlrep_getmeter(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_meter *pcm = m;
	char name[PSC_METER_NAME_MAX];
	struct psc_meter *pm;
	int rc, found, all;

	rc = 1;
	found = 0;
	snprintf(name, sizeof(name), "%s", pcm->pcm_mtr.pm_name);
	all = (name[0] == '\0');
	PLL_LOCK(&psc_meters);
	PLL_FOREACH(pm, &psc_meters)
		if (all || strncmp(pm->pm_name, name,
		    strlen(name)) == 0) {
			found = 1;

			pcm->pcm_mtr = *pm; /* XXX atomic */
			pcm->pcm_mtr.pm_max = *pm->pm_maxp; /* XXX atomic */
			rc = psc_ctlmsg_sendv(fd, mh, pcm);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(pm->pm_name, name) == 0)
				break;
		}
	PLL_ULOCK(&psc_meters);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown meter: %s", name);
	return (rc);
}

/**
 * psc_ctlrep_getmlist - Respond to a "GETMLIST" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine and reuse.
 */
int
psc_ctlrep_getmlist(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_mlist *pcml = m;
	char name[PEXL_NAME_MAX];
	struct psc_mlist *pml;
	int rc, found, all;

	rc = 1;
	found = 0;
	snprintf(name, sizeof(name), "%s", pcml->pcml_name);
	all = (name[0] == '\0');
	PLL_LOCK(&psc_mlists);
	PLL_FOREACH(pml, &psc_mlists)
		if (all || strncmp(pml->pml_name, name,
		    strlen(name)) == 0) {
			found = 1;

			MLIST_LOCK(pml);
			snprintf(pcml->pcml_name,
			    sizeof(pcml->pcml_name),
			    "%s", pml->pml_name);
			pcml->pcml_size = pml->pml_nitems;
			pcml->pcml_nseen = pml->pml_nseen;
			pcml->pcml_nwaiters =
			    psc_multiwaitcond_nwaiters(&pml->pml_mwcond_empty);
			MLIST_ULOCK(pml);

			rc = psc_ctlmsg_sendv(fd, mh, pcml);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(pml->pml_name, name) == 0)
				break;
		}
	PLL_ULOCK(&psc_mlists);
	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown mlist: %s", name);
	return (rc);
}

/**
 * psc_ctlrep_getodtable - Respond to a "GETODTABLE" control inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_getodtable(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_odtable *pco = m;
	struct odtable *odt;
	char name[ODT_NAME_MAX];
	int rc, found, all;

	rc = 1;
	found = 0;
	strlcpy(name, pco->pco_name, sizeof(name));
	all = (name[0] == '0');

	PLL_LOCK(&psc_odtables);
	PLL_FOREACH(odt, &psc_odtables) {
		if (all || strncmp(name,
		    odt->odt_name, strlen(name)) == 0) {
			found = 1;

			snprintf(pco->pco_name, sizeof(pco->pco_name),
			    "%s", odt->odt_name);
			pco->pco_elemsz = odt->odt_hdr->odth_elemsz;
			pco->pco_opts = odt->odt_hdr->odth_options;
			psc_vbitmap_getstats(odt->odt_bitmap,
			    &pco->pco_inuse, &pco->pco_total);
			rc = psc_ctlmsg_sendv(fd, mh, pco);
			if (!rc)
				break;

			/* Terminate on exact match. */
			if (strcmp(odt->odt_name, name) == 0)
				break;
		}
	}
	PLL_ULOCK(&psc_odtables);

	if (rc && !found && !all)
		rc = psc_ctlsenderr(fd, mh, "unknown odtable: %s",
		    name);
	return (rc);
}

/**
 * psc_ctlrep_getrpcsvc - Respond to a "GETRPCSVC" control inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_getrpcsvc(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_rpcsvc *pcrs = m;
	struct pscrpc_service *s;
	int rc;

	rc = 1;

	spinlock(&pscrpc_all_services_lock);
	psclist_for_each_entry(s, &pscrpc_all_services, srv_lentry) {
		spinlock(&s->srv_lock);
		strlcpy(pcrs->pcrs_name, s->srv_name,
		    sizeof(pcrs->pcrs_name));
		pcrs->pcrs_rqptl = s->srv_req_portal;
		pcrs->pcrs_rpptl = s->srv_rep_portal;
		pcrs->pcrs_rqsz = s->srv_max_req_size;;
		pcrs->pcrs_rpsz = s->srv_max_reply_size;
		pcrs->pcrs_bufsz = s->srv_buf_size;
		pcrs->pcrs_nbufs = s->srv_nbufs;
		pcrs->pcrs_nque = s->srv_n_queued_reqs;
		pcrs->pcrs_nact = s->srv_n_active_reqs;
		pcrs->pcrs_nthr = s->srv_nthreads;
		pcrs->pcrs_nrep = atomic_read(&s->srv_outstanding_replies);
		pcrs->pcrs_nrqbd = s->srv_nrqbd_receiving;
		pcrs->pcrs_nwq = psc_waitq_nwaiters(&s->srv_waitq);
		if (s->srv_count_peer_qlens)
			pcrs->pcrs_flags |= PSCRPC_SVCF_COUNT_PEER_QLENS;
		freelock(&s->srv_lock);

		rc = psc_ctlmsg_sendv(fd, mh, pcrs);
		if (!rc)
			break;
	}
	freelock(&pscrpc_all_services_lock);
	return (rc);
}

__weak struct psc_lockedlist *
pfl_journals_get(void)
{
	return (NULL);
}

/**
 * psc_ctlrep_getjournal - Respond to a "GETJOURNAL" control inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
int
psc_ctlrep_getjournal(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_journal *pcj = m;
	struct psc_lockedlist *pll;
	struct psc_journal *j;
	int rc;

	rc = 1;

	pll = pfl_journals_get();
	if (pll == NULL)
		return (rc);
	PLL_LOCK(pll);
	PLL_FOREACH(j, pll) {
		PJ_LOCK(j);
		strlcpy(pcj->pcj_name, j->pj_name,
		    sizeof(pcj->pcj_name));
		pcj->pcj_flags		= j->pj_flags;
		pcj->pcj_inuse		= j->pj_inuse;
		pcj->pcj_total		= j->pj_total;
		pcj->pcj_resrv		= j->pj_resrv;
		pcj->pcj_lastxid	= j->pj_lastxid;
		pcj->pcj_commit_txg	= j->pj_commit_txg;
		pcj->pcj_replay_xid	= j->pj_replay_xid;
		pcj->pcj_dstl_xid	= j->pj_distill_xid;
		pcj->pcj_pndg_xids_cnt	= pll_nitems(&j->pj_pendingxids);
		pcj->pcj_dstl_xids_cnt	= pll_nitems(&j->pj_distillxids);
		pcj->pcj_bufs_cnt	= psc_dynarray_len(&j->pj_bufs);
		pcj->pcj_nwaiters	= psc_waitq_nwaiters(&j->pj_waitq);
		pcj->pcj_nextwrite	= j->pj_nextwrite;
		pcj->pcj_wraparound	= j->pj_wraparound;
		PJ_ULOCK(j);

		rc = psc_ctlmsg_sendv(fd, mh, pcj);
		if (!rc)
			break;
	}
	PLL_ULOCK(pll);
	return (rc);
}

/**
 * psc_ctl_applythrop - Invoke an operation on all applicable threads.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine.
 * @thrname: name of thread to match on.
 * @cbf: callback to run for matching threads.
 */
int
psc_ctl_applythrop(int fd, struct psc_ctlmsghdr *mh, void *m,
    const char *thrname, int (*cbf)(int, struct psc_ctlmsghdr *, void *,
      struct psc_thread *))
{
	struct psc_thread *thr;
	int rc, len, nsz, found;

	rc = 1;
	found = 0;
	PLL_LOCK(&psc_threads);
	if (strcasecmp(thrname, PCTHRNAME_EVERYONE) == 0 ||
	    thrname[0] == '\0') {
		psclist_for_each_entry(thr,
		    &psc_threads.pll_listhd, pscthr_lentry) {
			rc = cbf(fd, mh, m, thr);
			if (!rc)
				break;
		}
	} else {
		len = strlen(thrname);
		psclist_for_each_entry(thr,
		    &psc_threads.pll_listhd, pscthr_lentry) {
			nsz = strcspn(thr->pscthr_name, "0123456789");
			if (len && strncasecmp(thrname,
			    thr->pscthr_name, len) == 0 &&
			    (len <= nsz ||
			     len == (int)strlen(thr->pscthr_name))) {
				found = 1;
				rc = cbf(fd, mh, m, thr);
				if (!rc)
					break;
			}
		}
		if (!found)
			rc = psc_ctlsenderr(fd, mh,
			    "unknown thread: %s", thrname);
	}
	PLL_ULOCK(&psc_threads);
	return (rc);
}

/**
 * psc_ctlthr_service - Satisfy a client connection.
 * @fd: client socket descriptor.
 * @ct: control operation table.
 * @nops: number of operations in table.
 * @msiz: value-result of buffer size allocated to subsequent message
 *	processing.
 * @pm: value-result buffer for subsequent message processing.
 *
 * Notes: sched_yield() is not explicity called throughout this routine,
 * which has implications, advantages, and disadvantages.
 *
 * Implications: we run till we finish the client connection and the next
 * accept() puts us back to sleep, if no intervening system calls which
 * run in the meantime relinquish control to other threads.
 *
 * Advantages: it might be nice to block all threads so processing by
 * other threads doesn't happen while control messages which modify
 * operation are being processed.
 *
 * Disadvantages: if we sleep during processing of client connection,
 * we deny service to new clients.
 */
__static int
psc_ctlthr_service(int fd, const struct psc_ctlop *ct, int nops,
    size_t *msiz, void *pm)
{
	struct psc_ctlmsghdr mh;
	ssize_t n;
	void *m;

	m = *(void **)pm;

	n = recv(fd, &mh, sizeof(mh), MSG_WAITALL | PFL_MSG_NOSIGNAL);
	if (n == 0)
		return (EOF);
	if (n == -1) {
		if (errno == EPIPE || errno == ECONNRESET)
			return (EOF);
		if (errno == EINTR)
			return (0);
		psc_fatal("recvmsg");
	}

	if (n != sizeof(mh)) {
		psclog_notice("short recvmsg on psc_ctlmsghdr; "
		    "nbytes=%zd", n);
		return (0);
	}
	if (mh.mh_size > *msiz) {
		*msiz = mh.mh_size;
		m = *(void **)pm = psc_realloc(m, *msiz, 0);
	}

 again:
	if (mh.mh_size) {
		n = recv(fd, m, mh.mh_size, MSG_WAITALL |
		    PFL_MSG_NOSIGNAL);
		if (n == -1) {
			if (errno == EPIPE || errno == ECONNRESET)
				return (EOF);
			if (errno == EINTR)
				goto again;
			psc_fatal("recv");
		}
		if ((size_t)n != mh.mh_size) {
			psclog_warn("short recv on psc_ctlmsg contents; "
			    "got=%zu; expected=%zu",
			    n, mh.mh_size);
			return (EOF);
		}
	}
	if (mh.mh_type < 0 ||
	    mh.mh_type >= nops ||
	    ct[mh.mh_type].pc_op == NULL) {
		psc_ctlsenderr(fd, &mh,
		    "unrecognized psc_ctlmsghdr type; "
		    "type=%d size=%zu", mh.mh_type, mh.mh_size);
		return (0);
	}
	if (ct[mh.mh_type].pc_siz &&
	    ct[mh.mh_type].pc_siz != mh.mh_size) {
		psc_ctlsenderr(fd, &mh,
		    "invalid ctlmsg size; type=%d, size=%zu, want=%zu",
		    mh.mh_type, mh.mh_size, ct[mh.mh_type].pc_siz);
		return (0);
	}
	psc_ctlthr(pscthr_get())->pct_stat.nrecv++;
	if (!ct[mh.mh_type].pc_op(fd, &mh, m))
		return (EOF);
	return (0);
}

/**
 * psc_ctlacthr_main - Control thread connection acceptor.
 * @thr: thread.
 */
__dead void
psc_ctlacthr_main(struct psc_thread *thr)
{
	int s, fd;

	s = psc_ctlacthr(thr)->pcat_sock;
	for (;;) {
		fd = accept(s, NULL, NULL);
		if (fd == -1) {
			if (errno == EINTR) {
				usleep(100);
				continue;
			}
			psc_fatal("accept");
		}
		psc_ctlacthr(pscthr_get())->pcat_stat.nclients++;

		spinlock(&psc_ctl_clifds_lock);
		psc_dynarray_add(&psc_ctl_clifds, (void *)(long)fd);
		psc_waitq_wakeall(&psc_ctl_clifds_waitq);
		freelock(&psc_ctl_clifds_lock);
	}
	/* NOTREACHED */
}

__dead void
psc_ctlthr_mainloop(struct psc_thread *thr)
{
	const struct psc_ctlop *ct;
	size_t bufsiz = 0;
	void *buf = NULL;
	uint32_t rnd;
	int s, nops;

	ct = psc_ctlthr(thr)->pct_ct;
	nops = psc_ctlthr(thr)->pct_nops;
	for (;;) {
		spinlock(&psc_ctl_clifds_lock);
		if (psc_dynarray_len(&psc_ctl_clifds) == 0) {
			psc_waitq_wait(&psc_ctl_clifds_waitq,
			    &psc_ctl_clifds_lock);
			continue;
		}
		rnd = psc_random32u(psc_dynarray_len(&psc_ctl_clifds));
		s = (int)(long)psc_dynarray_getpos(&psc_ctl_clifds, rnd);
		psc_dynarray_remove(&psc_ctl_clifds, (void *)(long)s);
		freelock(&psc_ctl_clifds_lock);

		if (!psc_ctlthr_service(s, ct, nops, &bufsiz, &buf)) {
			spinlock(&psc_ctl_clifds_lock);
			psc_dynarray_add(&psc_ctl_clifds, (void *)(long)s);
			psc_waitq_wakeall(&psc_ctl_clifds_waitq);
			freelock(&psc_ctl_clifds_lock);
		} else
			close(s);
	}
	PSCFREE(buf);
}

/**
 * psc_ctlthr_main - Main control thread client service loop.
 * @ofn: path to control socket.
 * @ct: control operations.
 * @nops: number of operations in @ct table.
 * @acthrtype: control acceptor thread type.
 */
__dead void
psc_ctlthr_main(const char *ofn, const struct psc_ctlop *ct, int nops,
    int acthrtype)
{
	extern const char *progname;
	struct psc_thread *thr, *me;
	struct sockaddr_un saun;
	mode_t old_umask;
	const char *p;
	int i, s;

	me = pscthr_get();

	p = strstr(me->pscthr_name, "ctlthr");
	if (p == NULL)
		psc_fatalx("'ctlthr' not found in control thread name");

	if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_LOCAL;

	/* preform transliteration for "variables" in file path */
	(void)FMTSTR(saun.sun_path, sizeof(saun.sun_path), ofn,
		FMTSTRCASE('h', "s", psclog_getdata()->pld_hostshort)
		FMTSTRCASE('n', "s", progname)
	);

	if (unlink(saun.sun_path) == -1 && errno != ENOENT)
		psclog_error("unlink %s", saun.sun_path);

	spinlock(&psc_umask_lock);
	old_umask = umask(S_IXUSR | S_IXGRP | S_IWOTH | S_IROTH |
	    S_IXOTH);
	if (bind(s, (struct sockaddr *)&saun, sizeof(saun)) == -1)
		psc_fatal("bind %s", saun.sun_path);
	umask(old_umask);
	freelock(&psc_umask_lock);

	/* XXX fchmod */
	if (chmod(saun.sun_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
	    S_IROTH | S_IWOTH) == -1)
		psc_fatal("chmod %s", saun.sun_path); /* XXX errno */

	if (listen(s, QLEN) == -1)
		psc_fatal("listen");

	/*
	 * Spawn a servicing thread to separate processing from acceptor
	 * and to multiplex between clients for fairness.
	 */
	thr = pscthr_init(acthrtype, 0, psc_ctlacthr_main,
	    NULL, sizeof(struct psc_ctlacthr), "%.*sctlacthr",
	    p - me->pscthr_name, me->pscthr_name);
	psc_ctlacthr(thr)->pcat_sock = s;
	pscthr_setready(thr);

#define PFL_CTL_NTHRS 4
	for (i = 1; i < PFL_CTL_NTHRS; i++) {
		thr = pscthr_init(me->pscthr_type, 0,
		    psc_ctlthr_mainloop, NULL,
		    sizeof(struct psc_ctlthr), "%.*sctlthr%d",
		    p - me->pscthr_name, me->pscthr_name, i);
		psc_ctlthr(thr)->pct_ct = ct;
		psc_ctlthr(thr)->pct_nops = nops;
		pscthr_setready(thr);
	}

	psc_ctlthr(me)->pct_ct = ct;
	psc_ctlthr(me)->pct_nops = nops;
	psc_ctlthr_mainloop(me);
}
