/* $Id$ */

/*
 * Control interface for querying and modifying
 * parameters of a running daemon instance.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/atomic.h"
#include "psc_ds/hash.h"
#include "psc_ds/list.h"
#include "psc_ds/listcache.h"
#include "psc_util/cdefs.h"
#include "psc_util/thread.h"
#include "psc_util/threadtable.h"

struct psc_thread pscControlThread;

#define Q 15	/* listen() queue */

/*
 * psc_ctlmsg_sendv - send a control message back to client.
 * @fd: client socket descriptor.
 * @mh: already filled-out control message header.
 * @m: control message contents.
 */
void
psc_ctlmsg_sendv(int fd, const struct psc_ctlmsghdr *mh, const void *m)
{
	struct iovec iov[2];
	size_t tsiz;
	ssize_t n;

	iov[0].iov_base = (void *)mh;
	iov[0].iov_len = sizeof(*mh);

	iov[1].iov_base = (void *)m;
	iov[1].iov_len = mh->mh_size;

	n = writev(fd, iov, NENTRIES(iov));
	if (n == -1)
		psc_fatal("write");
	tsiz = sizeof(*mh) + mh->mh_size;
	if ((size_t)n != tsiz)
		warn("short write");
	psc_ctlthr(&pscControlThread)->pc_st_nsent++;
	sched_yield();
}

/*
 * psc_ctlmsg_send - send a control message back to client.
 * @fd: client socket descriptor.
 * @type: type of message.
 * @siz: size of message.
 * @m: control message contents.
 * Notes: a control message header will be constructed and
 * written to the client preceding the message contents.
 */
void
psc_ctlmsg_send(int fd, int type, size_t siz, const void *m)
{
	struct psc_ctlmsghdr mh;
	struct iovec iov[2];
	size_t tsiz;
	ssize_t n;

	memset(&mh, 0, sizeof(mh));
	mh.mh_type = type;
	mh.mh_size = siz;

	iov[0].iov_base = &mh;
	iov[0].iov_len = sizeof(mh);

	iov[1].iov_base = (void *)m;
	iov[1].iov_len = siz;

	n = writev(fd, iov, NENTRIES(iov));
	if (n == -1)
		psc_fatal("write");
	tsiz = sizeof(mh) + siz;
	if ((size_t)n != tsiz)
		warn("short write");
	psc_ctlthr(&pscControlThread)->pc_st_nsent++;
	sched_yield();
}

/*
 * psc_ctlsenderr - send an error to client over control interface.
 * @fd: client socket descriptor.
 * @fmt: printf(3) format of error message.
 */
void
psc_ctlsenderr(int fd, struct psc_ctlmsghdr *mh, const char *fmt, ...)
{
	struct psc_ctlmsg_error pce;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(pce.pce_errmsg, sizeof(pce.pce_errmsg), fmt, ap); /* XXX */
	va_end(ap);

	mh->mh_type = PCMT_ERRMSG;
	mh->mh_size = sizeof(pce);
	psc_ctlmsg_sendv(fd, mh, &pce);
}

/*
 * psc_ctlthr_stat - export control thread stats.
 * @thr: thread begin queried.
 * @pcst: thread stats control message to be filled in.
 */
void
psc_ctlthr_stat(struct psc_thread *thr, struct psc_ctlmsg_stats *pcst)
{
	pcst->pcst_nclients = psc_ctlthr(thr)->pc_st_nclients;
	pcst->pcst_nsent    = psc_ctlthr(thr)->pc_st_nsent;
	pcst->pcst_nrecv    = psc_ctlthr(thr)->pc_st_nrecv;
}

/*
 * psc_ctlmsg_stats_send - send a response to a "getstats" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @pcst: thread stats control message to be filled in and sent out.
 * @thr: thread begin queried.
 */
__static void
psc_ctlmsg_stats_send(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_stats *pcst, struct psc_thread *thr)
{
	if (thr->pscthr_stat == NULL)
		return;
	snprintf(pcst->pcst_thrname, sizeof(pcst->pcst_thrname),
	    "%s", thr->pscthr_name);
	pcst->pcst_thrtype = thr->pscthr_type;
	thr->pscthr_stat(thr, pcst);
	psc_ctlmsg_sendv(fd, mh, pcst);
}

/*
 * psc_ctlrep_getstats - send a response to a "getstats" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine and reuse.
 */
void
psc_ctlrep_getstats(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_stats *pcst = m;

	psc_ctl_applythrop(fd, mh, m, pcst->pcst_thrname,
	    psc_ctlmsg_stats_send);
}

/*
 * psc_ctlrep_getsubsys - send a response to a "getsubsys" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 */
__static void
psc_ctlrep_getsubsys(int fd, struct psc_ctlmsghdr *mh, __unused void *m)
{
	struct psc_ctlmsg_subsys *pcss;
	const char **ss;
	size_t siz;
	int n;

	siz = PCSS_NAME_MAX * psc_nsubsys;
	pcss = PSCALLOC(siz);
	ss = dynarray_get(&psc_subsystems);
	for (n = 0; n < psc_nsubsys; n++)
		if (snprintf(&pcss->sss_names[n * PCSS_NAME_MAX],
		    PCSS_NAME_MAX, "%s", ss[n]) == -1) {
			psc_warn("snprintf");
			psc_ctlthr_senderrmsg(fd, mh,
			    "unable to retrieve subsystems");
			goto done;
		}
	mh->mh_size = siz;
	psc_ctlmsg_sendv(fd, mh, pcss);
 done:
	mh->mh_size = 0;	/* reset because we used our own buffer */
	free(pcss);
}

/*
 * psc_ctlmsg_getloglevel_send - send a response to a "getloglevel" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @thr: thread begin queried.
 */
__static void
psc_ctlmsg_loglevel_send(int fd, struct psc_ctlmsghdr *mh, struct psc_thread *thr)
{
	struct psc_ctlmsg_loglevel *pcl;
	size_t siz;

	siz = sizeof(*pll) + sizeof(*pll->pll_levels) * psc_nsubsys;
	pll = PSCALLOC(siz);
	snprintf(pll->pll_thrname, sizeof(pll->pll_thrname),
	    "%s", thr->pscthr_name);
	memcpy(pll->pll_levels, thr->pscthr_loglevels, psc_nsubsys *
	    sizeof(*pll->pll_levels));
	mh->mh_size = siz;
	psc_ctlmsg_sendv(fd, mh, pll);
	mh->mh_size = 0;	/* reset because we used our own buffer */
	free(pcl);
}

/*
 * psc_ctlmsg_getloglevel_send - send a response to a "getloglevel" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to examine.
 */
__static void
psc_ctlrep_getloglevel(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_loglevel *pcl = m;

	psc_ctl_applythrop(fd, mh, m, pcl->pcl_thrname,
	    psc_ctlmsg_loglevel_send);
}

/*
 * psc_ctlrep_gethashtable - respond to a "gethashtable" inquiry.
 *	This computes bucket usage statistics of a hash table and
 *	sends the results back to the client.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: control message to be filled in and sent out.
 */
__static void
psc_ctlrep_gethashtable(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_hashtable *pcht = m;
	struct hash_table *ht;
	char name[HTNAME_MAX];
	int found, all;

	snprintf(name, sizeof(name), "%s", pcht->pcht_name); /* XXX */
	all = (strcmp(name, PCHT_NAME_ALL) == 0);

	found = 0;
	spinlock(&hashTablesListLock);
	psclist_for_each_entry(ht, &hashTablesList, htable_entry) {
		if (all || strcmp(name, ht->htable_name) == 0) {
			found = 1;
			snprintf(pcht->pcht_name, sizeof(pcht->pcht_name),
			    "%s", ht->htable_name);
			hash_table_stats(ht, &pcht->pcht_totalbucks,
			    &pcht->pcht_usedbucks, &pcht->pcht_nents,
			    &pcht->pcht_maxbucklen);
			psc_ctlmsg_sendv(fd, mh, pcht);
			if (!all)
				break;
		}
	}
	freelock(&hashTablesListLock);
	if (!found && !all)
		psc_ctlsenderr(fd, mh, "unknown hash table: %s", name);
}

/*
 * psc_ctlmsg_lc_send - send a psc_ctlmsg_lc for a listcache.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @pclc: control message to be filled in and sent out.
 * @lc: the locked list_cache about which to reply with information.
 */
__static void
psc_ctlmsg_lc_send(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_lc *pclc, list_cache_t *lc)
{
	snprintf(pclc->pclc_name, sizeof(pclc->pclc_name),
	    "%s", lc->lc_name);
	pclc->pclc_size = lc->lc_size;
	pclc->pclc_max = lc->lc_max;
	pclc->pclc_nseen = lc->lc_nseen;
	LIST_CACHE_ULOCK(lc);
	psc_ctlmsg_sendv(fd, mh, pclc);
}

/*
 * psc_ctlrep_getlc - send a response to a "getlc" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @pclc: control message to examine and reuse.
 */
__static void
psc_ctlrep_getlc(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_lc *pclc = m;
	list_cache_t *lc;

	if (strcmp(pclc->slc_name, PCLC_NAME_ALL) == 0) {
		spinlock(&pscListCachesLock);
		psclist_for_each_entry(lc, &pscListCaches,
		    lc_index_lentry) {
			LIST_CACHE_LOCK(lc); /* XXX deadlock */
			psc_ctlmsg_lc_send(fd, mh, pclc, lc);
		}
		freelock(&pscListCachesLock);
	} else {
		lc = lc_lookup(pclc->pclc_name);
		if (lc)
			psc_ctlmsg_lc_send(fd, mh, pclc, lc);
		else
			psc_ctlsenderr(fd, mh,
			    "unknown listcache: %s",
			    pclc->slc_name);
	}
}

#define MAX_LEVELS 8

__static void
psc_ctlthr_rep_param(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *sp, const char *thrname,
    char **levels, int nlevels, const char *value)
{
	char *s, othrname[PSC_THRNAME_MAX];
	const char *p, *end;
	int lvl;

	snprintf(othrname, sizeof(othrname), "%s", sp->sp_thrname);
	snprintf(sp->sp_thrname, sizeof(sp->sp_thrname), "%s", thrname);

	s = sp->sp_field;
	end = s + sizeof(sp->sp_field) - 1;
	for (lvl = 0; s < end && lvl < nlevels; lvl++) {
		for (p = levels[lvl]; s < end && *p; s++, p++)
			*s = *p;
		if (s < end && lvl < nlevels - 1)
			*s++ = '.';
	}
	*s = '\0';

	snprintf(sp->sp_value, sizeof(sp->sp_value), "%s", value);
	psc_ctlmsg_sendv(fd, mh, sp);

	snprintf(sp->sp_thrname, sizeof(sp->sp_thrname), "%s", othrname);
}

__static void
psc_ctlthr_param_log_level(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *sp, char **levels, int nlevels)
{
	int n, nthr, set, loglevel, subsys, start_ss, end_ss;
	struct psc_thread **threads, *thr;

	levels[0] = "log";
	levels[1] = "level";

	loglevel = 0; /* gcc */
	threads = dynarray_get(&pscThreads);
	nthr = dynarray_len(&pscThreads);

	set = (mh->mh_type == SCMT_SETPARAM);

	if (set) {
		loglevel = psclog_id(sp->sp_value);
		if (loglevel == -1) {
			psc_ctlthr_senderrmsg(fd, mh,
			    "invalid log.level value: %s", sp->sp_value);
			return;
		}
	}

	if (nlevels == 3) {
		/* Subsys specified, use it. */
		subsys = psc_subsys_id(levels[2]);
		if (subsys == -1) {
			psc_ctlthr_senderrmsg(fd, mh,
			    "invalid log.level subsystem: %s", levels[2]);
			return;
		}
		start_ss = subsys;
		end_ss = subsys + 1;
	} else {
		/* No subsys specified, use all. */
		start_ss = 0;
		end_ss = psc_nsubsys;
	}

	FOR_EACH_THREAD(n, thr, sp->sp_thrname, threads, nthr)
		for (subsys = start_ss; subsys < end_ss; subsys++) {
			levels[2] = psc_subsys_name(subsys);
			if (set)
				thr->pscthr_loglevels[subsys] = loglevel;
			else {
				psc_ctlthr_rep_param(fd, mh, sp,
				    thr->pscthr_name, levels, 3,
				    psclog_name(thr->pscthr_loglevels[subsys]));
			}
		}
}

__static void
psc_ctlrep_param(int fd, struct psc_ctlmsghdr *mh,
    struct psc_ctlmsg_param *pcp)
{
	char *t, *levels[MAX_LEVELS];
	int nlevels, set;

	set = (mh->mh_type == SCMT_SETPARAM);

	for (nlevels = 0, t = sp->sp_field;
	    nlevels < MAX_LEVELS && (levels[nlevels] = t) != NULL;
	    nlevels++) {
		if ((t = strchr(levels[nlevels], '.')) != NULL)
			*t++ = '\0';
		if (*levels[nlevels] == '\0')
			goto invalid;
	}

	if (nlevels == 0 || nlevels >= MAX_LEVELS)
		goto invalid;

	if (strcmp(levels[0], "log") == 0) {
		if (nlevels == 1) {
			if (set)
				goto invalid;
			psc_ctlthr_param_log_level(fd, mh, sp, levels, nlevels);
		} else if (strcmp(levels[1], "level") == 0)
			psc_ctlthr_param_log_level(fd, mh, sp, levels, nlevels);
		else
			goto invalid;
	} else
		goto invalid;
	return;

 invalid:
	while (nlevels > 1)
		levels[--nlevels][-1] = '.';
	psc_ctlsenderr(fd, mh,
	    "invalid field/value: %s", pcp->pcp_field);
}

/*
 * psc_ctlrep_iostats - send a response to a "getiostats" inquiry.
 * @fd: client socket descriptor.
 * @mh: already filled-in control message header.
 * @m: iostats control message to be filled in and sent out.
 */
__static void
psc_ctlrep_iostats(int fd, struct psc_ctlmsghdr *mh, void *m)
{
	struct psc_ctlmsg_iostats *pci = m;
	char name[IST_NAME_MAX];
	struct iostats *ist;
	int found, all;

	snprintf(name, sizeof(name), "%s", pci->pci_ist.ist_name);
	all = (strcmp(name, PCI_NAME_ALL) == 0);

	found = 0;
	spinlock(&iostatsListLock);
	psclist_for_each_entry(ist, &iostatsList, ist_lentry)
		if (all ||
		    strncmp(ist->ist_name, name, strlen(name)) == 0) {
			found = 1;
			pci->pci_ist = *ist;
			psc_ctlmsg_sendv(fd, mh, pci);
			if (strlen(ist->ist_name) == strlen(name))
				break;
		}
	freelock(&iostatsListLock);

	if (!found && !all)
		psc_ctlsenderr(fd, mh,
		    "unknown iostats: %s", name);
}

void
psc_ctl_applythrop(int fd, struct psc_ctlmsghdr *mh, void *m, const char *thrname,
    void (*cb)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *))
{
	int n, nthr;

	/* XXX lock or snapshot threads so they don't change underneath us */
	nthr = dynarray_len(&pscThreads);
	threads = dynarray_get(&pscThreads);
	if (strcasecmp(thrname, PCTHRNAME_EVERYONE) == 0) {
		for (n = 0; n < nthr; n++)
			cb(fd, mh, m, threads[n]);
	} else {
		for (n = 0; n < nthr; n++)
			if (strcasecmp(thrname,
			    threads[n]->pscthr_name) == 0) {
				cb(fd, mh, m, threads[n]);
				break;
			}
		if (n == nthr)
			psc_ctlsenderr(fd, mh,
			    "unknown thread: %s", thrname);
	}
}

/*
 * psc_ctlthr_service - satisfy a client connection.
 * @fd: client socket descriptor.
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
 * Disadvantages: if we don't go to sleep during processing of client
 * connection, anyone can denial the service quite easily.
 */
__static void
psc_ctlthr_service(int fd, struct psc_ctlops *ct, int nops)
{
	struct psc_ctlmsghdr mh;
	size_t siz;
	ssize_t n;
	void *m;

	m = NULL;
	siz = 0;
	while ((n = read(fd, &mh, sizeof(mh))) != -1 && n != 0) {
		if (n != sizeof(mh)) {
			psc_notice("short read on psc_ctlmsghdr; read=%zd", n);
			continue;
		}
		if (mh.mh_size > msiz) {
			siz = mh.mh_size;
			if ((m = realloc(m, siz)) == NULL)
				psc_fatal("realloc");
		}
		n = read(fd, m, mh.mh_size);
		if (n == -1)
			psc_fatal("read");
		if ((size_t)n != mh.mh_size) {
			psc_warn("short read on psc_ctlmsg contents; "
			    "read=%zu; expected=%zu",
			    n, mh.mh_size);
			break;
		}
		if (mh.mh_type < 0 ||
		    mh.mh_type >= nops ||
		    ct[mh.mh_type].pc_op == NULL) {
			psc_warnx("unrecognized psc_ctlmsghdr type; "
			    "type=%d size=%zu", mh.mh_type, mh.mh_size);
			psc_ctlthr_senderrmsg(fd, mh,
			    "unrecognized psc_ctlmsghdr type; "
			    "type=%d size=%zu", mh.mh_type, mh.mh_size);
			continue;
		}
		if (ct[mh.mh_type].pc_siz &&
		    ct[mh.mh_type].pc_siz != mh->mh_size) {
			psc_ctlthr_senderrmsg(fd, mh,
			    "invalid ctlmsg size; type=%d, size=%zu",
			    mh->mh_type, mh->mh_size);
			continue;
		}
		ct[mh.mh_type].pc_op(fd, mh, m);
	}
	if (n == -1)
		psc_fatal("read");
	free(m);
}

/*
 * psc_ctlthr_main - main control thread client-servicing loop.
 * @fn: path to control socket.
 * @ct: control operations.
 * @nops: number of operations in @ct table.
 */
__dead void
psc_ctlthr_main(const char *fn, struct psc_ctlops *ct, int nops)
{
	struct sockaddr_un sun;
	mode_t old_umask;
	sigset_t sigset;
	socklen_t siz;
	int rc, s, fd;

	/* Ignore SIGPIPEs in this thread. */
	if (sigemptyset(&sigset) == -1)
		psc_fatal("sigemptyset");
	if (sigaddset(&sigset, SIGPIPE) == -1)
		psc_fatal("sigemptyset");
	rc = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	if (rc)
		psc_fatalx("pthread_sigmask: %s", strerror(rc));

	/* Create control socket. */
	if ((s = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", fn);
	if (unlink(fn) == -1)
		if (errno != ENOENT)
			psc_error("unlink %s", fn);

	old_umask = umask(S_IXUSR | S_IXGRP | S_IWOTH | S_IROTH | S_IXOTH);
	if (bind(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		psc_fatal("bind %s", fn);
	umask(old_umask);

	/* XXX fchmod */
	if (chmod(fn, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
	    S_IROTH | S_IWOTH) == -1) {
		unlink(fn);
		psc_fatal("chmod %s", fn); /* XXX errno */
	}

	/* Serve client connections. */
	if (listen(s, Q) == -1)
		psc_fatal("listen");

	for (;;) {
		siz = sizeof(sun);
		if ((fd = accept(s, (struct sockaddr *)&sun,
		    &siz)) == -1)
			psc_fatal("accept");
		psc_ctlthr(&pscControlThread)->sc_st_nclients++;
		psc_ctlthr_service(fd, ct, nops);
		close(fd);
	}
	/* NOTREACHED */
}
