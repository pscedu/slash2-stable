/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include <err.h>
#include <inttypes.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/getopt.h"
#include "pfl/str.h"
#include "pfl/subsys.h"
#include "psc_ds/list.h"
#include "psc_ds/vbitmap.h"
#include "psc_rpc/rpc.h"
#include "psc_util/ctl.h"
#include "psc_util/ctlcli.h"
#include "psc_util/fmt.h"
#include "psc_util/fmtstr.h"
#include "psc_util/log.h"
#include "psc_util/meter.h"
#include "psc_util/pool.h"
#include "psc_util/thread.h"

#define PCTHRT_RD 0
#define PCTHRT_WR 1

int			  psc_ctl_inhuman;
int			  psc_ctl_lastmsgtype = -1;
struct psc_ctlmsghdr	 *psc_ctl_msghdr;
int			  psc_ctl_nodns;
int			  psc_ctl_noheader;
int			  psc_ctl_nsubsys;
volatile sig_atomic_t	  psc_ctl_saw_winch = 1;
__static int		  psc_ctl_sock;
const char		 *psc_ctl_sockfn;
char			**psc_ctl_subsys_names;
psc_spinlock_t		  psc_ctl_lock = SPINLOCK_INIT;

__static void
psc_ctlmsg_sendlast(void)
{
	ssize_t siz;

	spinlock(&psc_ctl_lock);
	if (psc_ctl_sock == -1) {
		freelock(&psc_ctl_lock);
		return;
	}

	/* Send last queued control messages. */
	siz = psc_ctl_msghdr->mh_size + sizeof(*psc_ctl_msghdr);
	if (write(psc_ctl_sock, psc_ctl_msghdr, siz) != siz)
		psc_fatal("write");
	freelock(&psc_ctl_lock);
}

__static struct psc_ctlshow_ent *
psc_ctlshow_lookup(const char *name)
{
	int n;

	if (strlen(name) == 0)
		return (NULL);
	for (n = 0; n < psc_ctlshow_ntabents; n++)
		if (strncasecmp(name, psc_ctlshow_tab[n].pse_name,
		    strlen(name)) == 0)
			return (&psc_ctlshow_tab[n]);
	return (NULL);
}

void *
psc_ctlmsg_push(int type, size_t msiz)
{
	static int id;
	size_t tsiz;

	if (psc_ctl_msghdr)
		psc_ctlmsg_sendlast();

	tsiz = msiz + sizeof(*psc_ctl_msghdr);
	psc_ctl_msghdr = psc_realloc(psc_ctl_msghdr, tsiz, PAF_NOLOG);
	memset(psc_ctl_msghdr, 0, tsiz);
	psc_ctl_msghdr->mh_type = type;
	psc_ctl_msghdr->mh_size = msiz;
	psc_ctl_msghdr->mh_id = id++;
	return (&psc_ctl_msghdr->mh_data);
}

void
psc_ctl_packshow_fault(char *thrs)
{
	struct psc_ctlmsg_fault *pcflt;
	char *thr, *fault, *next;
	int n;

	pcflt = psc_ctlmsg_push(PCMT_GETFAULT, sizeof(*pcflt));
	if (thrs) {
		fault = strchr(thrs, ':');
		if (fault)
			*fault++ = '\0';
		else
			thrs = PCTHRNAME_EVERYONE;

		n = strlcpy(pcflt->pcflt_name, fault,
		    sizeof(pcflt->pcflt_name));
		if (n == 0 || n >= (int)sizeof(pcflt->pcflt_name))
			errx(1, "invalid fault point name: %s", fault);

		for (thr = thrs; thr; thr = next, pcflt = NULL) {
			next = strchr(thr, ';');
			if (next)
				*next++ = '\0';

			if (pcflt == NULL) {
				pcflt = psc_ctlmsg_push(PCMT_GETFAULT,
				    sizeof(*pcflt));
				strlcpy(pcflt->pcflt_name, fault,
				    sizeof(pcflt->pcflt_name));
			}

			n = strlcpy(pcflt->pcflt_thrname, PCTHRNAME_EVERYONE,
			    sizeof(pcflt->pcflt_thrname));
			if (n == 0 || n >= (int)sizeof(pcflt->pcflt_thrname))
				errx(1, "invalid thread name: %s", thr);
		}
	}
}

void
psc_ctl_packshow_hashtable(char *table)
{
	struct psc_ctlmsg_hashtable *pcht;
	int n;

	pcht = psc_ctlmsg_push(PCMT_GETHASHTABLE, sizeof(*pcht));
	if (table) {
		n = strlcpy(pcht->pcht_name, table,
		    sizeof(pcht->pcht_name));
		if (n == 0 || n >= (int)sizeof(pcht->pcht_name))
			errx(1, "invalid hash table name: %s", table);
	}
}

void
psc_ctl_packshow_iostats(char *iostats)
{
	struct psc_ctlmsg_iostats *pci;
	int n;

	pci = psc_ctlmsg_push(PCMT_GETIOSTATS, sizeof(*pci));
	if (iostats) {
		n = strlcpy(pci->pci_ist.ist_name, iostats,
		    sizeof(pci->pci_ist.ist_name));
		if (n == 0 || n >= (int)sizeof(pci->pci_ist.ist_name))
			errx(1, "invalid iostats name: %s", iostats);
	}
}

void
psc_ctl_packshow_journal(char *journal)
{
	struct psc_ctlmsg_journal *pcj;
	int n;

	pcj = psc_ctlmsg_push(PCMT_GETJOURNAL, sizeof(*pcj));
	if (journal) {
		n = strlcpy(pcj->pcj_name, journal,
		    sizeof(pcj->pcj_name));
		if (n == 0 || n >= (int)sizeof(pcj->pcj_name))
			errx(1, "invalid journal name: %s", journal);
	}
}

void
psc_ctl_packshow_listcache(char *lc)
{
	struct psc_ctlmsg_lc *pclc;
	int n;

	pclc = psc_ctlmsg_push(PCMT_GETLC, sizeof(*pclc));
	if (lc) {
		n = strlcpy(pclc->pclc_name, lc,
		    sizeof(pclc->pclc_name));
		if (n == 0 || n >= (int)sizeof(pclc->pclc_name))
			errx(1, "invalid list cache name: %s", lc);
	}
}

void
psc_ctl_packshow_lni(char *lni)
{
	struct psc_ctlmsg_lni *pclni;

	pclni = psc_ctlmsg_push(PCMT_GETLNI, sizeof(*pclni));
	if (lni) {
//		n = strlcpy(pclni->pclni_name, lni,
//		    sizeof(pclni->pclni_name));
//		if (n == 0 || n >= (int)sizeof(pclni->pclni_name))
//			errx(1, "invalid lnet if name: %s", lc);
	}
}

void
psc_ctl_packshow_loglevels(char *thr)
{
	struct psc_ctlmsg_loglevel *pcl;
	int n;

	psc_ctlmsg_push(PCMT_GETSUBSYS,
	    sizeof(struct psc_ctlmsg_subsys));

	pcl = psc_ctlmsg_push(PCMT_GETLOGLEVEL, sizeof(*pcl));
	if (thr) {
		n = strlcpy(pcl->pcl_thrname, thr, sizeof(pcl->pcl_thrname));
		if (n == 0 || n >= (int)sizeof(pcl->pcl_thrname))
			errx(1, "invalid thread name: %s", thr);
	}
}

void
psc_ctl_packshow_meter(char *meter)
{
	struct psc_ctlmsg_meter *pcm;
	int n;

	pcm = psc_ctlmsg_push(PCMT_GETMETER, sizeof(*pcm));
	if (meter) {
		n = strlcpy(pcm->pcm_mtr.pm_name, meter,
		    sizeof(pcm->pcm_mtr.pm_name));
		if (n == 0 || n >= (int)sizeof(pcm->pcm_mtr.pm_name))
			errx(1, "invalid progress meter name: %s", meter);
	}
}

void
psc_ctl_packshow_mlist(char *mlist)
{
	struct psc_ctlmsg_mlist *pcml;
	int n;

	pcml = psc_ctlmsg_push(PCMT_GETMLIST, sizeof(*pcml));
	if (mlist) {
		n = strlcpy(pcml->pcml_name, mlist,
		    sizeof(pcml->pcml_name));
		if (n == 0 || n >= (int)sizeof(pcml->pcml_name))
			errx(1, "invalid mlist name: %s", mlist);
	}
}

void
psc_ctl_packshow_odtable(char *table)
{
	struct psc_ctlmsg_odtable *pco;
	int n;

	pco = psc_ctlmsg_push(PCMT_GETODTABLE, sizeof(*pco));
	if (table) {
		n = strlcpy(pco->pco_name, table, sizeof(pco->pco_name));
		if (n == 0 || n >= (int)sizeof(pco->pco_name))
			errx(1, "invalid odtable name: %s", table);
	}
}

void
psc_ctl_packshow_pool(char *pool)
{
	struct psc_ctlmsg_pool *pcpl;
	int n;

	pcpl = psc_ctlmsg_push(PCMT_GETPOOL, sizeof(*pcpl));
	if (pool) {
		n = strlcpy(pcpl->pcpl_name, pool,
		    sizeof(pcpl->pcpl_name));
		if (n == 0 || n >= (int)sizeof(pcpl->pcpl_name))
			errx(1, "invalid pool name: %s", pool);
	}
}

void
psc_ctl_packshow_rpcsvc(char *rpcsvc)
{
	struct psc_ctlmsg_rpcsvc *pcrs;
	int n;

	pcrs = psc_ctlmsg_push(PCMT_GETRPCSVC, sizeof(*pcrs));
	if (rpcsvc) {
		n = strlcpy(pcrs->pcrs_name, rpcsvc, sizeof(pcrs->pcrs_name));
		if (n == 0 || n >= (int)sizeof(pcrs->pcrs_name))
			errx(1, "invalid rpcsvc name: %s", rpcsvc);
	}
}

void
psc_ctl_packshow_thread(char *thr)
{
	struct psc_ctlmsg_thread *pcst;
	int n;

	pcst = psc_ctlmsg_push(PCMT_GETTHREAD, sizeof(*pcst));
	if (thr) {
		n = strlcpy(pcst->pcst_thrname, thr, sizeof(pcst->pcst_thrname));
		if (n == 0 || n >= (int)sizeof(pcst->pcst_thrname))
			errx(1, "invalid thread name: %s", thr);
	}
}

void
psc_ctlparse_show(char *showspec)
{
	char *items, *item, *next;
	struct psc_ctlshow_ent *pse;
	int n;

	if (strcmp(showspec, "?") == 0) {
		warnx("available show specs:");

		for (n = 0; n < psc_ctlshow_ntabents; n++)
			warnx("  %s", psc_ctlshow_tab[n].pse_name);
		exit(1);
	}

	items = strchr(showspec, ':');
	if (items)
		*items++ = '\0';

	pse = psc_ctlshow_lookup(showspec);
	if (pse == NULL)
		errx(1, "invalid show parameter: %s", showspec);

	if (items == NULL)
		pse->pse_cb(NULL);
	else {
		for (item = items; item; item = next) {
			next = strchr(item, ',');
			if (next)
				*next++ = '\0';
			pse->pse_cb(item);
		}
	}
}

void
psc_ctlparse_lc(char *lists)
{
	struct psc_ctlmsg_lc *pclc;
	char *list, *listnext;
	int n;

	psclog_warnx("-L is deprecated, use -sL");
	for (list = lists; list != NULL; list = listnext) {
		if ((listnext = strchr(list, ',')) != NULL)
			*listnext++ = '\0';

		pclc = psc_ctlmsg_push(PCMT_GETLC, sizeof(*pclc));

		n = snprintf(pclc->pclc_name, sizeof(pclc->pclc_name),
		    "%s", list);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pclc->pclc_name))
			errx(1, "invalid list: %s", list);
	}
}

void
psc_ctlparse_param(char *spec)
{
	struct psc_ctlmsg_param *pcp;
	char *thr, *field, *value;
	int flags, n;

	flags = 0;
	if ((value = strchr(spec, '=')) != NULL) {
		if (value > spec && value[-1] == '+') {
			flags = PCPF_ADD;
			value[-1] = '\0';
		} else if (value > spec && value[-1] == '-') {
			flags = PCPF_SUB;
			value[-1] = '\0';
		}
		*value++ = '\0';
	}

	if ((field = strchr(spec, '.')) == NULL) {
		thr = PCTHRNAME_EVERYONE;
		field = spec;
	} else {
		*field = '\0';

		if (strstr(spec, "thr") == NULL) {
			/*
			 * No thread specified:
			 * assume global or everyone.
			 */
			thr = PCTHRNAME_EVERYONE;
			*field = '.';
			field = spec;
		} else {
			/*
			 * We saw "thr" at the first level:
			 * assume thread specification.
			 */
			thr = spec;
			field++;
		}
	}

	pcp = psc_ctlmsg_push(value ? PCMT_SETPARAM : PCMT_GETPARAM,
	    sizeof(*pcp));

	/* Set thread name. */
	n = snprintf(pcp->pcp_thrname, sizeof(pcp->pcp_thrname), "%s", thr);
	if (n == -1)
		psc_fatal("snprintf");
	else if (n == 0 || n > (int)sizeof(pcp->pcp_thrname))
		errx(1, "invalid thread name: %s", thr);

	/* Set parameter name. */
	n = snprintf(pcp->pcp_field, sizeof(pcp->pcp_field),
	    "%s", field);
	if (n == -1)
		psc_fatal("snprintf");
	else if (n == 0 || n > (int)sizeof(pcp->pcp_field))
		errx(1, "invalid parameter: %s", thr);

	/* Set parameter value (if applicable). */
	if (value) {
		pcp->pcp_flags = flags;
		n = snprintf(pcp->pcp_value,
		    sizeof(pcp->pcp_value), "%s", value);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pcp->pcp_value))
			errx(1, "invalid parameter value: %s", thr);
	}
}

void
psc_ctlparse_pool(char *pools)
{
	struct psc_ctlmsg_pool *pcpl;
	char *pool, *poolnext;

	psclog_warnx("-P is deprecated, use -sP");
	for (pool = pools; pool; pool = poolnext) {
		if ((poolnext = strchr(pool, ',')) != NULL)
			*poolnext++ = '\0';

		pcpl = psc_ctlmsg_push(PCMT_GETPOOL, sizeof(*pcpl));
		if (strlcpy(pcpl->pcpl_name, pool,
		    sizeof(pcpl->pcpl_name)) >= sizeof(pcpl->pcpl_name))
			errx(1, "invalid pool: %s", pool);
	}
}

void
psc_ctlparse_iostats(char *iostats)
{
	struct psc_ctlmsg_iostats *pci;
	char *iostat, *next;
	int n;

	psclog_warnx("-I is deprecated, use -sI");
	for (iostat = iostats; iostat != NULL; iostat = next) {
		if ((next = strchr(iostat, ',')) != NULL)
			*next++ = '\0';

		pci = psc_ctlmsg_push(PCMT_GETIOSTATS, sizeof(*pci));

		n = snprintf(pci->pci_ist.ist_name,
		    sizeof(pci->pci_ist.ist_name), "%s", iostat);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pci->pci_ist.ist_name))
			errx(1, "invalid iostat name: %s", iostat);
	}
}

int
psc_ctl_loglevel_namelen(int n)
{
	size_t maxlen;
	int j;

	maxlen = strlen(psc_ctl_subsys_names[n]);
	for (j = 0; j < PNLOGLEVELS; j++)
		maxlen = MAX(maxlen, strlen(psc_loglevel_getname(j)));
	return (maxlen);
}

void
psc_ctlthr_pr(const struct psc_ctlmsg_thread *pcst)
{
	printf(" #sent %9u #recv %9u #drop %9u",
	    pcst->pcst_nsent, pcst->pcst_nrecv,
	    pcst->pcst_ndrop);
}

void
psc_ctlacthr_pr(const struct psc_ctlmsg_thread *pcst)
{
	printf(" #conn %8u", pcst->pcst_nclients);
}

void
psc_ctlmsg_hashtable_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-30s %5s %7s %7s "
	    "%6s %6s %6s %6s\n",
	    "hash-table", "flags", "#fill", "#bkts",
	    "%fill", "#ents", "avglen", "maxlen");
}

void
psc_ctlmsg_hashtable_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_hashtable *pcht = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	psc_fmt_ratio(rbuf, pcht->pcht_usedbucks, pcht->pcht_totalbucks);
	printf("%-30s    %c%c "
	    "%7d %7d "
	    "%6s %6d "
	    "%6.1f "
	    "%6d\n",
	    pcht->pcht_name,
	    pcht->pcht_flags & PHTF_RESORT ? 'R' : '-',
	    pcht->pcht_flags & PHTF_STR ? 'S' : '-',
	    pcht->pcht_usedbucks, pcht->pcht_totalbucks,
	    rbuf, pcht->pcht_nents,
	    pcht->pcht_nents * 1.0 / pcht->pcht_totalbucks,
	    pcht->pcht_maxbucklen);
}

void
psc_ctlmsg_error_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_error *pce = m;

	if (psc_ctl_lastmsgtype != mh->mh_type &&
	    psc_ctl_lastmsgtype != -1)
		fprintf(stderr, "\n");
	warnx("%s", pce->pce_errmsg);
}

int
psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_subsys *pcss = m;
	int n;

	if (mh->mh_size == 0 ||
	    mh->mh_size % PCSS_NAME_MAX)
		return (sizeof(*pcss));

	/* Release old subsystems. */
	for (n = 0; n < psc_ctl_nsubsys; n++)
		PSCFREE(psc_ctl_subsys_names[n]);
	PSCFREE(psc_ctl_subsys_names);

	psc_ctl_nsubsys = mh->mh_size / PCSS_NAME_MAX;
	psc_ctl_subsys_names = PSCALLOC(psc_ctl_nsubsys *
	    sizeof(*psc_ctl_subsys_names));
	for (n = 0; n < psc_ctl_nsubsys; n++) {
		psc_ctl_subsys_names[n] = PSCALLOC(PCSS_NAME_MAX);
		memcpy(psc_ctl_subsys_names[n],
		    &pcss->pcss_names[n * PCSS_NAME_MAX], PCSS_NAME_MAX);
		psc_ctl_subsys_names[n][PCSS_NAME_MAX - 1] = '\0';
	}
	mh->mh_type = psc_ctl_lastmsgtype;	/* hack to fix newline */
	return (0);
}

void
psc_ctlmsg_iostats_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-47s %10s %10s %10s\n",
	    "I/O-stat", "rate10s", "ratecur", "total");
}

void
psc_ctlmsg_iostats_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_iostats *pci = m;
	const struct psc_iostats *ist = &pci->pci_ist;
	char buf[PSCFMT_HUMAN_BUFSIZ];
	double d;
	int i;

	printf("%-47s ", ist->ist_name);
	for (i = IST_NINTV - 1; i >= 0; i--) {
		d = psc_iostats_getintvrate(ist, i);

		if (psc_ctl_inhuman)
			printf("%10.2f ", d);
		else {
			psc_fmt_human(buf, d);
			printf("%8s/s ", buf);
		}
	}
	if (psc_ctl_inhuman)
		printf("%10"PRIu64, ist->ist_len_total);
	else {
		psc_fmt_human(buf, ist->ist_len_total);
		printf("%10s", buf);
	}
	printf("\n");
}

void
psc_ctlmsg_journal_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-10s %4s %4s %6s %3s "
	    "%13s %13s %13s %6s %5s\n",
	    "journal", "flag", "used", "total", "rsv",
	    "last-xid", "commit-txg", "distill-xid",
	    "nxtslt", "wraps");
}

void
psc_ctlmsg_journal_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_journal *pcj = m;

	printf("%-10s %c%c%c%c %4u %6u %3u "
	    "%13"PRIx64" %13"PRIx64" %13"PRIx64" "
	    "%6d %5"PRId64"\n",
	    pcj->pcj_name,
	    pcj->pcj_flags & PJF_WANTBUF	? 'B' : '-',
	    pcj->pcj_flags & PJF_WANTSLOT	? 'S' : '-',
	    pcj->pcj_flags & PJF_ISBLKDEV	? 'B' : '-',
	    pcj->pcj_flags & PJF_REPLAYINPROG	? 'R' : '-',
	    pcj->pcj_inuse, pcj->pcj_total, pcj->pcj_resrv,
	    pcj->pcj_lastxid, pcj->pcj_commit_txg, pcj->pcj_dstl_xid,
	    pcj->pcj_nextwrite, pcj->pcj_wraparound);
}

void
psc_ctlmsg_meter_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-24s %18s %s\n",
	    "progress-meter", "position", "progress");
}

void
psc_ctlmsg_meter_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_meter *pcm = m;
	uint64_t max;
	int n = 0, len;
	char buf[50];

	max = pcm->pcm_mtr.pm_max;
	if (pcm->pcm_mtr.pm_cur > max)
		max = pcm->pcm_mtr.pm_cur;

	n = snprintf(buf, sizeof(buf), "%"PRIu64"/%"PRIu64,
	    pcm->pcm_mtr.pm_cur, max);
	len = printf("%-24s %*s%s ", pcm->pcm_mtr.pm_name, 18 - n, "",
	    buf);
	psc_assert(len != -1);
	n = 0;

	putchar('|');
	len = PSC_CTL_DISPLAY_WIDTH - len - 3;
	if (len < 0)
		len = 0;
	if (max) {
		for (; n < (int)(len * pcm->pcm_mtr.pm_cur / max); n++)
			putchar('=');
		putchar(pcm->pcm_mtr.pm_cur == max ? '=' : '>');
	} else
		putchar(' ');
	for (; n < len; n++)
		putchar(' ');
	printf("|\n");
}

void
psc_ctlmsg_pool_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-12s %3s %6s %6s %6s "
	    "%6s %6s %6s %2s "
	    "%10s %3s %3s\n",
	    "mem-pool", "flg", "#free", "#use", "total",
	    "%use", "min", "max", "th",
	    "#shrnx", "#em", "#wa");
	/* XXX add ngets and waiting/sleep time */
}

void
psc_ctlmsg_pool_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_pool *pcpl = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	psc_fmt_ratio(rbuf, pcpl->pcpl_total - pcpl->pcpl_free,
	    pcpl->pcpl_total);
	printf("%-12s %c%c%c "
	    "%6d %6d "
	    "%6d %6s",
	    pcpl->pcpl_name,
	    pcpl->pcpl_flags & PPMF_AUTO	? 'A' : '-',
	    pcpl->pcpl_flags & PPMF_PIN		? 'P' : '-',
	    pcpl->pcpl_flags & PPMF_MLIST	? 'M' : '-',
	    pcpl->pcpl_free, pcpl->pcpl_total - pcpl->pcpl_free,
	    pcpl->pcpl_total, rbuf);
	if (pcpl->pcpl_flags & PPMF_AUTO) {
		printf(" %6d ", pcpl->pcpl_min);
		if (pcpl->pcpl_max)
			printf("%6d", pcpl->pcpl_max);
		else
			printf("%6s", "<inf>");
		printf(" %2d", pcpl->pcpl_thres);
	} else
		printf(" %6s %6s %2s", "-", "-", "-");

	if (pcpl->pcpl_flags & PPMF_AUTO)
		printf(" %10"PRIu64, pcpl->pcpl_nshrink);
	else
		printf(" %10s", "-");

	printf(" %3d", pcpl->pcpl_nw_empty);
	if (pcpl->pcpl_flags & PPMF_MLIST)
		printf("   -");
	else
		printf(" %3d", pcpl->pcpl_nw_want);
	printf("\n");
}

void
psc_ctlmsg_lc_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-43s %3s %8s %3s %3s %15s\n",
	    "list-cache", "flg", "#items", "#wa", "#em", "#seen");
}

void
psc_ctlmsg_lc_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_lc *pclc = m;

	printf("%-43s   %c "
	    "%8"PRIu64" %3d %3d %15"PRIu64"\n",
	    pclc->pclc_name,
	    pclc->pclc_flags & PLCF_DYING ? 'D' : '-',
	    pclc->pclc_size,
	    pclc->pclc_nw_want, pclc->pclc_nw_empty,
	    pclc->pclc_nseen);
}

void
psc_ctlmsg_param_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-36s %s\n",
	    "parameter", "value");
}

void
psc_ctlmsg_param_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_param *pcp = m;

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) == 0)
		printf("%-36s %s\n", pcp->pcp_field, pcp->pcp_value);
	else
		printf("%s.%-*s %s\n", pcp->pcp_thrname,
		    36 - (int)strlen(pcp->pcp_thrname) - 1,
		    pcp->pcp_field, pcp->pcp_value);
}

void
psc_ctlmsg_thread_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	const char *msg = "thread-specific-stats";
	int n;

	n = printf("%-16s %5s"
#ifdef HAVE_NUMA
	    " %6s"
#endif
	    " %3s ",
	    "thread", "thrid",
#ifdef HAVE_NUMA
	    "memnid",
#endif
	    "flg");
	printf("%*s%s\n", (PSC_CTL_DISPLAY_WIDTH - n - 1) / 2 -
	    (int)strlen(msg) / 2, "", msg);
}

void
psc_ctlmsg_thread_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_thread *pcst = m;

	printf("%-16s %5d"
#ifdef HAVE_NUMA
	    " %6d"
#endif
	    " %c%c%c",
	    pcst->pcst_thrname, pcst->pcst_thrid,
#ifdef HAVE_NUMA
	    pcst->pcst_memnode,
#endif
	    pcst->pcst_flags & PTF_PAUSED	? 'P' : '-',
	    pcst->pcst_flags & PTF_RUN		? 'R' : '-',
	    pcst->pcst_flags & PTF_READY	? 'I' : '-');
	if (pcst->pcst_thrtype < psc_ctl_nprthrs &&
	    psc_ctl_prthrs[pcst->pcst_thrtype])
		psc_ctl_prthrs[pcst->pcst_thrtype](pcst);
	printf("\n");
}

void
psc_ctlmsg_lni_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-39s %8s %8s %8s %8s %4s\n",
	    "lnetif", "maxcr", "txcr", "mincr", "peercr", "refs");
}

void
psc_ctlmsg_lni_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_lni *pclni = m;

	printf("%-39s %8d %8d %8d %8d %4d\n",
	    pclni->pclni_nid, pclni->pclni_maxtxcredits,
	    pclni->pclni_txcredits, pclni->pclni_mintxcredits,
	    pclni->pclni_peertxcredits, pclni->pclni_refcount);
}

int
psc_ctlmsg_loglevel_check(struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	__unusedx struct psc_ctlmsg_loglevel *pcl;

	if (mh->mh_size != sizeof(*pcl) +
	    psc_ctl_nsubsys * sizeof(*pcl->pcl_levels))
		return (sizeof(*pcl));
	return (0);
}

void
psc_ctlmsg_loglevel_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	int n;

	n = PSC_THRNAME_MAX - (int)strlen("thread") -
	    (int)strlen("loglevel:") - 1;
	if (n < 0)
		n = 0;
	printf("thread %*s loglevel:", n, "");
	for (n = 0; n < psc_ctl_nsubsys; n++)
		printf(" %*s", psc_ctl_loglevel_namelen(n),
		    psc_ctl_subsys_names[n]);
	printf("\n");
}

void
psc_ctlmsg_loglevel_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_loglevel *pcl = m;
	int n;

	printf("%-*s ", PSC_THRNAME_MAX, pcl->pcl_thrname);
	for (n = 0; n < psc_ctl_nsubsys; n++)
		printf(" %*s", psc_ctl_loglevel_namelen(n),
		    psc_loglevel_getname(pcl->pcl_levels[n]));
	printf("\n");
}

void
psc_ctlmsg_mlist_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-51s %8s %3s %15s\n",
	    "mlist", "#items", "#em", "#seen");
}

void
psc_ctlmsg_mlist_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_mlist *pcml = m;

	printf("%-51s %8d %3d %15"PRIu64"\n",
	    pcml->pcml_name, pcml->pcml_size,
	    pcml->pcml_nwaiters, pcml->pcml_nseen);
}

void
psc_ctlmsg_fault_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-36s %3s %7s %7s %5s "
	    "%10s %5s %5s %5s\n",
	    "fault-point", "flg", "#hit", "#uhit", "delay",
	    "count", "begin", "code", "prob");
}

void
psc_ctlmsg_fault_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_fault *pcflt = m;

	printf("%-36s   %c "
	    "%7d %7d %5d "
	    "%10d %5d %5d "
	    "%5d\n",
	    pcflt->pcflt_name,
	    pcflt->pcflt_flags & PFLTF_ACTIVE ? 'A' : '-',
	    pcflt->pcflt_hits, pcflt->pcflt_unhits, pcflt->pcflt_delay,
	    pcflt->pcflt_count, pcflt->pcflt_begin, pcflt->pcflt_retval,
	    pcflt->pcflt_chance);
}

void
psc_ctlmsg_odtable_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-46s %3s %6s %7s %7s %6s\n",
	    "on-disk-table", "flg", "elemsz", "inuse", "total", "%use");
}

void
psc_ctlmsg_odtable_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_odtable *pco = m;
	char buf[PSCFMT_RATIO_BUFSIZ];

	psc_fmt_ratio(buf, pco->pco_inuse, pco->pco_total);
	printf("%-46s  %c%c "
	    "%6d %7d %7d %6s\n",
	    pco->pco_name,
	    pco->pco_opts & ODTBL_OPT_CRC	? 'c' : '-',
	    pco->pco_opts & ODTBL_OPT_SYNC	? 's' : '-',
	    pco->pco_elemsz, pco->pco_inuse, pco->pco_total, buf);
}

void
psc_ctlmsg_rpcsvc_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-9s %3s "
	    "%4s %4s %5s "
	    "%4s %5s %5s "
	    "%4s %4s %4s "
	    "%5s %6s %5s\n",
	    "rpcsvc", "flg",
	    "rqsz", "rpsz", "bufsz",
	    "#buf", "rqptl", "rpptl",
	    "#thr", "#que", "#act",
	    "#wait", "#outrp", "nrqbd");
}

void
psc_ctlmsg_rpcsvc_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_rpcsvc *pcrs = m;

	printf("%-9s   %c "
	    "%4d %4d %5d "
	    "%4d %5u %5u "
	    "%4d %4d %4d "
	    "%5d %6d %5d\n",
	    pcrs->pcrs_name,
	    pcrs->pcrs_flags & PSCRPC_SVCF_COUNT_PEER_QLENS ? 'Q' : '-',
	    pcrs->pcrs_rqsz, pcrs->pcrs_rpsz, pcrs->pcrs_bufsz,
	    pcrs->pcrs_nbufs, pcrs->pcrs_rqptl, pcrs->pcrs_rpptl,
	    pcrs->pcrs_nthr, pcrs->pcrs_nque, pcrs->pcrs_nact,
	    pcrs->pcrs_nwq, pcrs->pcrs_nrep, pcrs->pcrs_nrqbd);
}

__static void
psc_ctlmsg_print(struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_prfmt *prf;
	int n;

	/* Validate message type. */
	if (mh->mh_type < 0 ||
	    mh->mh_type >= psc_ctlmsg_nprfmts)
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	prf = &psc_ctlmsg_prfmts[mh->mh_type];

	/* Validate message size. */
	if (prf->prf_msgsiz) {
		if (prf->prf_msgsiz != mh->mh_size)
			psc_fatalx("invalid ctlmsg size; type=%d; "
			    "sizeof=%zu expected=%zu", mh->mh_type,
			    mh->mh_size, prf->prf_msgsiz);
	} else if (prf->prf_check == NULL)
		/* Disallowed message type. */
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	else {
		n = prf->prf_check(mh, m);
		if (n == -1)
			return;
		else if (n)
			psc_fatalx("invalid ctlmsg size; type=%d sizeof=%zu "
			    "expected=%d", mh->mh_type, mh->mh_size, n);
	}

	/* Print display header. */
	if (!psc_ctl_noheader && psc_ctl_lastmsgtype != mh->mh_type &&
	    prf->prf_prhdr != NULL) {

		if (psc_ctl_lastmsgtype != -1)
			printf("\n");
		prf->prf_prhdr(mh, m);
		for (n = 0; n < PSC_CTL_DISPLAY_WIDTH; n++)
			putchar('=');
		putchar('\n');
	}

	/* Print display contents. */
	if (prf->prf_prdat)
		prf->prf_prdat(mh, m);
	psc_ctl_lastmsgtype = mh->mh_type;
}

void
psc_ctl_read(int s, void *buf, size_t siz)
{
	ssize_t n;

	while (siz) {
		n = read(s, buf, siz);
		if (n == -1)
			psc_fatal("read");
		else if (n == 0)
			psc_fatalx("received unexpected EOF from daemon");
		siz -= n;
		buf += n;
	}
}

void
psc_ctlcli_rd_main(__unusedx struct psc_thread *thr)
{
	struct psc_ctlmsghdr mh;
	ssize_t n, siz;
	void *m;
	int s;

	/* Read and print response messages. */
	m = NULL;
	siz = 0;
	while ((n = read(psc_ctl_sock, &mh, sizeof(mh))) != -1 && n != 0) {
		if (n != sizeof(mh)) {
			psclog_warnx("short read");
			continue;
		}
		if (mh.mh_size == 0)
			psc_fatalx("received invalid message from daemon");
		if (mh.mh_size >= (size_t)siz) {
			siz = mh.mh_size;
			m = psc_realloc(m, siz, 0);
		}
		psc_ctl_read(psc_ctl_sock, m, mh.mh_size);
		psc_ctlmsg_print(&mh, m);
		sched_yield();
	}
	if (n == -1)
		psc_fatal("read");
	psc_free(m, 0);

	spinlock(&psc_ctl_lock);
	s = psc_ctl_sock;
	psc_ctl_sock = -1;
	freelock(&psc_ctl_lock);

	close(psc_ctl_sock);
}

int
matches_cmd(const char *cmd, const char *ucmd)
{
	size_t n;

	if (strchr(cmd, ':')) {
		n = strcspn(cmd, ":");
		if (n != strcspn(ucmd, ":="))
			return (0);
		return (strncasecmp(cmd, ucmd, n) == 0);
	}
	return (strcasecmp(cmd, ucmd) == 0);
}

extern void usage(void);

void
psc_ctlcli_main(const char *osockfn, int ac, char *av[],
    const struct psc_ctlopt *otab, int notab)
{
	extern const char *daemon_name, *progname;
	char optstr[LINE_MAX], chbuf[3], *p;
	struct psc_thread *thr;
	struct sockaddr_un sun;
	const char *prg;
	int rc, c, i;

	prg = strrchr(progname, '/');
	if (prg)
		prg++;
	else
		prg = progname;

	pscthr_init(PCTHRT_WR, 0, NULL, NULL, 0, "%swrthr", prg);

	psc_ctl_sockfn = osockfn;

	p = getenv("CTL_SOCK_FILE");
	if (p)
		psc_ctl_sockfn = p;

	optstr[0] = '\0';
	chbuf[2] = '\0';
	strlcat(optstr, "+S:", sizeof(optstr));
	for (i = 0; i < notab; i++) {
		chbuf[0] = otab[i].pco_ch;
		chbuf[1] = otab[i].pco_type == PCOF_FLAG ? '\0' : ':';
		strlcat(optstr, chbuf, sizeof(optstr));
	}

	/* First pass through arguments for validity and sockfn. */
	while ((c = getopt(ac, av, optstr)) != -1) {
		if (c == 'S') {
			psc_ctl_sockfn = optarg;
			continue;
		}
		for (i = 0; i < notab; i++)
			if (c == otab[i].pco_ch)
				break;
		if (i == notab)
			usage();
	}

	/* Connect to control socket. */
	if ((psc_ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;

	FMTSTR(sun.sun_path, sizeof(sun.sun_path), psc_ctl_sockfn,
		FMTSTRCASE('h', "s", psclog_getdata()->pld_hostshort)
		FMTSTRCASE('n', "s", daemon_name)
	);

	if (connect(psc_ctl_sock, (struct sockaddr *)&sun,
	    sizeof(sun)) == -1)
		err(1, "connect: %s", sun.sun_path);

	thr = pscthr_init(PCTHRT_RD, 0, psc_ctlcli_rd_main, NULL, 0,
	    "%srdthr", prg);

	/* Parse options for real this time. */
	PFL_OPT_RESET();
	while ((c = getopt(ac, av, optstr)) != -1) {
		for (i = 0; i < notab; i++) {
			if (c != otab[i].pco_ch)
				continue;
			switch (otab[i].pco_type) {
			case PCOF_FLAG:
				*(int *)otab[i].pco_data = 1;
				break;
			case PCOF_FUNC:
				((void (*)(const char *))otab[i].pco_data)(optarg);
				break;
			}
			break;
		}
	}
	ac -= optind;
	av += optind;
	if (ac) {
		for (i = 0; i < psc_ctlcmd_nreqs; i++)
			if (matches_cmd(psc_ctlcmd_reqs[i].pccr_name,
			    av[0])) {
				psc_ctlcmd_reqs[i].pccr_cbf(ac, av);
				break;
			}
		if (i == psc_ctlcmd_nreqs)
			errx(1, "unrecognized command: %s", av[0]);
	}

	if (psc_ctl_msghdr == NULL)
		errx(1, "no actions specified");

	psc_ctlmsg_sendlast();

	spinlock(&psc_ctl_lock);
	if (psc_ctl_sock != -1 &&
	    shutdown(psc_ctl_sock, SHUT_WR) == -1)
		psc_fatal("shutdown");
	freelock(&psc_ctl_lock);

	rc = pthread_join(thr->pscthr_pthread, NULL);
	if (rc)
		psc_fatalx("pthread_join: %s", strerror(rc));
}

int
psc_ctl_get_display_maxwidth(void)
{
	static int width = PSC_CTL_DISPLAY_WIDTH;
	struct winsize ws;
	FILE *ttyfp;

	if (psc_ctl_saw_winch) {
		psc_ctl_saw_winch = 0;

		ttyfp = fopen(_PATH_TTY, "r");
		if (ttyfp) {
			if (ioctl(fileno(ttyfp), TIOCGWINSZ, &ws) == 0 &&
			    ws.ws_col >= PSC_CTL_DISPLAY_WIDTH)
				width = ws.ws_col;
			fclose(ttyfp);
		}
	}
	return (width);
}
