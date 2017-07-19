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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include <curses.h>
#include <err.h>
#include <inttypes.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/ctl.h"
#include "pfl/ctlcli.h"
#include "pfl/fmt.h"
#include "pfl/fmtstr.h"
#include "pfl/getopt.h"
#include "pfl/list.h"
#include "pfl/log.h"
#include "pfl/meter.h"
#include "pfl/net.h"
#include "pfl/pool.h"
#include "pfl/rpc.h"
#include "pfl/str.h"
#include "pfl/subsys.h"
#include "pfl/syspaths.h"
#include "pfl/thread.h"
#include "pfl/vbitmap.h"

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

int			  psc_ctl_hascolors;

void
setcolor(int col)
{
	if (!psc_ctl_hascolors || col == -1)
		return;
	putp(tparm(enter_bold_mode));
	putp(tparm(set_a_foreground, col));
}

void
uncolor(void)
{
	if (!psc_ctl_hascolors)
		return;
	putp(tparm(orig_pair));
	putp(tparm(exit_attribute_mode));
}

__static void
psc_ctlmsg_sendlast(void)
{
	ssize_t siz, rc;

	spinlock(&psc_ctl_lock);
	if (psc_ctl_sock == -1) {
		freelock(&psc_ctl_lock);
		return;
	}

	/* Send last queued control messages. */
	siz = psc_ctl_msghdr->mh_size + sizeof(*psc_ctl_msghdr);
	rc = write(psc_ctl_sock, psc_ctl_msghdr, siz);
	if (rc != siz) {
		/* 09/23/2016: Hit rc = -1 with msctl -R repl-status */
		psc_fatal("write, rc = %zd", rc);
	}
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
	size_t n;

	pcht = psc_ctlmsg_push(PCMT_GETHASHTABLE, sizeof(*pcht));
	if (table) {
		n = strlcpy(pcht->pcht_name, table,
		    sizeof(pcht->pcht_name));
		if (n == 0 || n >= sizeof(pcht->pcht_name))
			errx(1, "invalid hash table name: %s", table);
	}
}

void
psc_ctl_packshow_opstat(char *opstat)
{
	struct psc_ctlmsg_opstat *pco;
	size_t n;

	pco = psc_ctlmsg_push(PCMT_GETOPSTATS, sizeof(*pco));
	if (opstat) {
		n = strlcpy(pco->pco_name, opstat,
		    sizeof(pco->pco_name));
		if (n == 0 || n >= sizeof(pco->pco_name))
			errx(1, "invalid opstat name: %s", opstat);
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
	struct psc_ctlmsg_listcache *pclc;
	int n;

	pclc = psc_ctlmsg_push(PCMT_GETLISTCACHE, sizeof(*pclc));
	if (lc) {
		n = strlcpy(pclc->pclc_name, lc,
		    sizeof(pclc->pclc_name));
		if (n == 0 || n >= (int)sizeof(pclc->pclc_name))
			errx(1, "invalid list cache name: %s", lc);
	}
}

void
psc_ctl_packshow_lnetif(char *lni)
{
	struct psc_ctlmsg_lnetif *pclni;

	pclni = psc_ctlmsg_push(PCMT_GETLNETIF, sizeof(*pclni));
	if (lni) {
//		n = strlcpy(pclni->pclni_name, lni,
//		    sizeof(pclni->pclni_name));
//		if (n == 0 || n >= (int)sizeof(pclni->pclni_name))
//			errx(1, "invalid lnet if name: %s", lc);
	}
}

void
psc_ctl_packshow_thread(char *thr)
{
	struct psc_ctlmsg_thread *pct;
	size_t n;

	psc_ctlmsg_push(PCMT_GETSUBSYS,
	    sizeof(struct psc_ctlmsg_subsys));

	pct = psc_ctlmsg_push(PCMT_GETTHREAD, sizeof(*pct));
	if (thr) {
		n = strlcpy(pct->pct_thrname, thr,
		    sizeof(pct->pct_thrname));
		if (n == 0 || n >= sizeof(pct->pct_thrname))
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
psc_ctl_packshow_rpcrq(__unusedx char *rpcrq)
{
	struct psc_ctlmsg_rpcrq *pcrq;

	psc_ctlmsg_push(PCMT_GETRPCRQ, sizeof(*pcrq));
	/* XXX filter by xid or addr? */
}

void
psc_ctl_packshow_rpcsvc(char *rpcsvc)
{
	struct psc_ctlmsg_rpcsvc *pcrs;
	int n;

	pcrs = psc_ctlmsg_push(PCMT_GETRPCSVC, sizeof(*pcrs));
	if (rpcsvc) {
		n = strlcpy(pcrs->pcrs_name, rpcsvc,
		    sizeof(pcrs->pcrs_name));
		if (n == 0 || n >= (int)sizeof(pcrs->pcrs_name))
			errx(1, "invalid rpcsvc name: %s", rpcsvc);
	}
}

void
pfl_ctl_packshow_fsrq(__unusedx char *rpcrq)
{
	struct pfl_ctlmsg_fsrq *pcfr;

	psc_ctlmsg_push(PCMT_GETFSRQ, sizeof(*pcfr));
}

void
pfl_ctl_packshow_workrq(__unusedx char *rpcrq)
{
	struct pfl_ctlmsg_workrq *pcw;

	psc_ctlmsg_push(PCMT_GETWORKRQ, sizeof(*pcw));
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
	struct psc_ctlmsg_listcache *pclc;
	char *list, *listnext;
	int n;

	psclog_warnx("-L is deprecated, use -sL");
	for (list = lists; list != NULL; list = listnext) {
		if ((listnext = strchr(list, ',')) != NULL)
			*listnext++ = '\0';

		pclc = psc_ctlmsg_push(PCMT_GETLISTCACHE, sizeof(*pclc));

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

int
psc_ctlmsg_hashtable_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-30s %5s %7s %7s "
	    "%6s %6s %6s %6s\n",
	    "hash-table", "flags", "#fill", "#bkts",
	    "%fill", "#ents", "avglen", "maxlen");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_hashtable_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_hashtable *pcht = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	pfl_fmt_ratio(rbuf, pcht->pcht_usedbucks, pcht->pcht_totalbucks);
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

int
psc_ctlmsg_opstat_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-42s %13s %13s %13s %13s\n",
	    "opstat", "avg rate", "max rate", "cur rate", "total");
	return(PSC_CTL_DISPLAY_WIDTH+18);
}

void
psc_ctlmsg_opstat_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_opstat *pco = m;
	const struct pfl_opstat *opst = &pco->pco_opst;
	int base10 = 0;

	if (opst->opst_flags & OPSTF_BASE10 || psc_ctl_inhuman)
		base10 = 1;

	printf("%-42s ", pco->pco_name);

	// 11.2
	psc_ctl_prnumber(base10, opst->opst_avg, 11, "/s ");
	psc_ctl_prnumber(base10, opst->opst_max, 11, "/s ");
	psc_ctl_prnumber(base10, opst->opst_intv, 11, "/s ");
	psc_ctl_prnumber(base10, psc_atomic64_read(&opst->opst_lifetime), 13, "");
	printf("\n");
}

int
psc_ctlmsg_journal_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-15s %4s %4s %6s %2s "
	    "%9s %7s %8s "
	    "%6s %4s %5s\n",
	    "journal", "flag", "used", "total", "rs",
	    "lastxid", "comitxg", "distlxid",
	    "nxslot", "wrap", "nbufs");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_journal_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_journal *pcj = m;

	printf("%-15s %c%c%c%c %4u %6u %2u "
	    "%9"PRIx64" %7"PRIx64" %8"PRIx64" "
	    "%6d %4"PRId64" %5u\n",
	    pcj->pcj_name,
	    pcj->pcj_flags & PJF_WANTBUF	? 'B' : '-',
	    pcj->pcj_flags & PJF_WANTSLOT	? 'S' : '-',
	    pcj->pcj_flags & PJF_ISBLKDEV	? 'B' : '-',
	    pcj->pcj_flags & PJF_REPLAYINPROG	? 'R' : '-',
	    pcj->pcj_inuse, pcj->pcj_total, pcj->pcj_resrv,
	    pcj->pcj_lastxid, pcj->pcj_commit_txg, pcj->pcj_dstl_xid,
	    pcj->pcj_nextwrite, pcj->pcj_wraparound, pcj->pcj_bufs_cnt);
}

int
psc_ctlmsg_meter_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-24s %18s %s\n",
	    "progress-meter", "position", "progress");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_meter_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	char *p, buf[50], rbuf[PSCFMT_RATIO_BUFSIZ];
	const struct psc_ctlmsg_meter *pcm = m;
	uint64_t cur, max;
	int n = 0, len;

	cur = pcm->pcm_mtr.pm_cur;
	max = pcm->pcm_mtr.pm_max;
	if (cur > max)
		max = pcm->pcm_mtr.pm_cur;

	pfl_fmt_ratio(rbuf, cur, max);
	p = strchr(rbuf, '.');
	if (p) {
		p[0] = '%';
		p[1] = '\0';
	}

	n = snprintf(buf, sizeof(buf), "%"PRIu64"/%"PRIu64, cur, max);
	len = printf("%-24s %*s%s ", pcm->pcm_mtr.pm_name, 18 - n, "",
	    buf);
	psc_assert(len != -1);
	n = 0;

	putchar('|');
	len = PSC_CTL_DISPLAY_WIDTH - len - 3;
	if (len < 0)
		len = 0;
	if (max) {
		int nr = len * cur / max;
		int slen = strlen(rbuf) + 2;

		if (nr > slen + 1) {
			len -= slen;
			nr -= slen;
			for (; n < nr / 2; n++)
				putchar('=');
			printf(" %s ", rbuf);
			for (; n < nr; n++)
				putchar('=');
			putchar(cur == max ? '=' : '>');
		} else {
			for (; n < nr; n++)
				putchar('=');
			if (cur)
				putchar('>');
			printf(" %s ", rbuf);
			if (!cur)
				putchar(' ');
			len -= slen;
		}
	} else
		putchar(' ');
	for (; n < len; n++)
		putchar(' ');
	printf("|\n");
}

int
psc_ctlmsg_pool_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-12s %4s %8s %8s %8s "
	    "%7s %6s %6s %5s "
	    "%10s %3s %3s\n",
	    "mem-pool", "flag", "#free", "#use", "total",
	    "%use", "min", "max", "thrsh",
	    "#shrnx", "#em", "#wa");
	/* XXX add ngets and waiting/sleep time */
	return(PSC_CTL_DISPLAY_WIDTH+11);
}

void
psc_ctlmsg_pool_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_pool *pcpl = m;
	char rbuf[PSCFMT_RATIO_BUFSIZ];

	pfl_fmt_ratio(rbuf, pcpl->pcpl_total - pcpl->pcpl_free,
	    pcpl->pcpl_total);
	printf("%-12s  %c%c%c "
	    "%8d %8d "
	    "%8d %7s",
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
		printf(" %5d", pcpl->pcpl_thres);
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

int
psc_ctlmsg_listcache_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-43s %4s %8s  %5s  %6s %15s\n",
	    "list-cache", "flag", "#items", "#want", "#empty", "#seen");
	return(PSC_CTL_DISPLAY_WIDTH+8);
}

void
psc_ctlmsg_listcache_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_listcache *pclc = m;

	printf("%-43s    %c "
	    "%8"PRIu64"  %5d  %6d %15"PRIu64"\n",
	    pclc->pclc_name,
	    pclc->pclc_flags & PLCF_DYING ? 'D' : '-',
	    pclc->pclc_size,
	    pclc->pclc_nw_want,
	    pclc->pclc_nw_empty,
	    pclc->pclc_nseen);
}

int
psc_ctlmsg_param_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-55s %s\n",
	    "parameter", "value");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_param_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_param *pcp = m;

	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) == 0)
		printf("%-55s %s\n", pcp->pcp_field, pcp->pcp_value);
	else
		printf("%s.%-*s %s\n", pcp->pcp_thrname,
		    40 - (int)strlen(pcp->pcp_thrname) - 1,
		    pcp->pcp_field, pcp->pcp_value);
}

int
psc_ctlmsg_thread_check(struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	__unusedx struct psc_ctlmsg_thread *pct;

	if (mh->mh_size != sizeof(*pct) +
	    psc_ctl_nsubsys * sizeof(*pct->pct_loglevels))
		return (sizeof(*pct));
	return (0);
}

int
psc_ctlmsg_thread_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	const char *memnid = "";
	int n, memnid_len = 0;

#ifdef HAVE_NUMA
	memnid_len = 7;
	memnid = " memnid";
#endif

	printf("%26s %10s %24s    %3s%*s", "thread", "tid", "wait", "flg", 
	    memnid_len, memnid);

	for (n = 0; n < psc_ctl_nsubsys; n++)
		printf(" %.3s", psc_ctl_subsys_names[n]);
	printf("\n");

	return(PSC_CTL_DISPLAY_WIDTH+33);
}

int
pflctl_loglevel_abbr(int level)
{
	int c;

	if (level == PLL_MAX)
		return ('m');
	c = "fewnidbvt"[level];
	return (c);
}

void
psc_ctlmsg_thread_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_thread *pct = m;
	int n, ll;

	printf("%26s %10d %24s    %c%c%c",
	    pct->pct_thrname,
	    pct->pct_tid,
	    pct->pct_waitname[0] == '\0' ? "-----" : pct->pct_waitname,
	    pct->pct_flags & PTF_PAUSED	? 'P' : '-',
	    pct->pct_flags & PTF_RUN	? 'R' : '-',
	    pct->pct_flags & PTF_READY	? 'I' : '-');

#ifdef HAVE_NUMA
	printf(" %6d", pct->pct_memnode);
#endif

	for (n = 0; n < psc_ctl_nsubsys; n++) {
		ll = pct->pct_loglevels[n];
		printf(" %c:%d", pflctl_loglevel_abbr(ll), ll);
	}
	printf("\n");
}

int
psc_ctlmsg_lnetif_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-39s %8s %8s %8s %8s %4s\n",
	    "lnetif", "maxcr", "txcr", "mincr", "peercr", "refs");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_lnetif_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_lnetif *pclni = m;

	printf("%-39s %8d %8d %8d %8d %4d\n",
	    pclni->pclni_nid, pclni->pclni_maxtxcredits,
	    pclni->pclni_txcredits, pclni->pclni_mintxcredits,
	    pclni->pclni_peertxcredits, pclni->pclni_refcount);
}

int
psc_ctlmsg_mlist_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-51s %8s %3s %15s\n",
	    "mlist", "#items", "#em", "#seen");
	return(PSC_CTL_DISPLAY_WIDTH);
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

int
psc_ctlmsg_fault_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-29s %1s %5s %5s %5s "
	    "%9s %5s %3s %5s %4s\n",
	    "fault-point", "f", "hit", "unhit", "delay",
	    "count", "begin", "pro", "rc", "intv");

	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_fault_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_fault *pcflt = m;

	printf("%-29s %c "
	    "%5d %5d %5d "
	    "%9d %5d %3d "
	    "%5d %4d\n",
	    pcflt->pcflt_name,
	    pcflt->pcflt_flags & PFLTF_ACTIVE ? 'A' : '-',
	    pcflt->pcflt_hits, pcflt->pcflt_unhits, pcflt->pcflt_delay,
	    pcflt->pcflt_count, pcflt->pcflt_begin, pcflt->pcflt_chance,
	    pcflt->pcflt_retval, pcflt->pcflt_interval);
}

int
psc_ctlmsg_odtable_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-46s %3s %6s %7s %7s %6s\n",
	    "on-disk-table", "flg", "elemsz", "inuse", "total", "%use");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_odtable_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_odtable *pco = m;
	char buf[PSCFMT_RATIO_BUFSIZ];

	pfl_fmt_ratio(buf, pco->pco_inuse, pco->pco_total);
	printf("%-46s  %c "
	    "%6d %7d %7d %6s\n",
	    pco->pco_name,
	    pco->pco_opts & ODTBL_OPT_CRC	? 'c' : '-',
	    pco->pco_elemsz, pco->pco_inuse, pco->pco_total, buf);
}

void
pflctl_print_extra_field(int *remaining, const char *fmt, ...)
{
	va_list ap, apd;
	int n;

	va_start(ap, fmt);
	va_copy(apd, ap);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (n == -1)
		psclog_warn("formatting string");
	else if (*remaining > n) {
		vprintf(fmt, apd);
		va_end(apd);
		*remaining -= n;
	}
}

int
psc_ctlmsg_rpcrq_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	int remaining;

	remaining = psc_ctl_get_display_maxwidth() -
	    PSC_CTL_DISPLAY_WIDTH;

	pflctl_print_extra_field(&remaining, "%-16s ", "rpcrq-addr");

	/*
	 * We eliminate LNET from the NID to conserve display space
	 * since NIDs won't overlap in 99% of deployment circumstances
	 * as this is mostly a debugging feature.
	 */
	/* strlen(IPV4 address e.g. `xxx.xxx.xxx.xxx') = 15 */
	printf("%7s %2s %-20s %2s %4s "
	    "%15s %2s %2s %4s %4s "
	    "%3s %2s",
	    "rpc-xid", "rf", "flags", "op", "stat",
	    "peernid", "qp", "pp", "qlen", "plen",
	    "try", "nw");

	pflctl_print_extra_field(&remaining, " %4s", "nbrc");
	pflctl_print_extra_field(&remaining, " %2s", "tr");
	pflctl_print_extra_field(&remaining, "%2s", "ig");

	printf("\n");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_rpcrq_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_rpcrq *pcrq = m;
	int remaining;
	char *p;

	remaining = psc_ctl_get_display_maxwidth() -
	    PSC_CTL_DISPLAY_WIDTH;

	pflctl_print_extra_field(&remaining, "%016"PRIx64" ",
	    pcrq->pcrq_addr);

	p = strchr(pcrq->pcrq_peer, '@');
	if (p)
		*p = '\0';
	p = strchr(pcrq->pcrq_self, '@');
	if (p)
		*p = '\0';

	printf("%7"PRId64" %2d "
	    "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c "
	    "%2d %4d "
	    "%15s "
	    "%2d %2d "
	    "%4d %4d "
	    "%3d %2d",
	    pcrq->pcrq_xid, pcrq->pcrq_refcount,
	    pcrq->pcrq_type == PSCRPC_MSG_ERR ? 'e' :
	      pcrq->pcrq_type == PSCRPC_MSG_REQUEST ? 'q' :
	      pcrq->pcrq_type == PSCRPC_MSG_REPLY ? 'p' : '?',
	    pcrq->pcrq_phase >= PSCRPC_RQ_PHASE_NEW &&
	    pcrq->pcrq_phase <= PSCRPC_RQ_PHASE_COMPLETE ?
	      PSCRPC_PHASE_NAMES[pcrq->pcrq_phase -
		PSCRPC_RQ_PHASE_NEW] : '?',
	    pcrq->pcrq_abort_reply	? 'A' : '-',
	    pcrq->pcrq_bulk_abortable	? 'B' : '-',
	    pcrq->pcrq_err		? 'E' : '-',
	    pcrq->pcrq_has_bulk		? 'b' : '-',
	    pcrq->pcrq_has_intr		? 'i' : '-',
	    pcrq->pcrq_has_set		? 's' : '-',
	    pcrq->pcrq_intr		? 'I' : '-',
	    pcrq->pcrq_net_err		? 'e' : '-',
	    pcrq->pcrq_no_delay		? 'd' : '-',
	    pcrq->pcrq_no_resend	? 'N' : '-',
	    pcrq->pcrq_receiving_reply	? 'r' : '-',
	    pcrq->pcrq_replay		? 'P' : '-',
	    pcrq->pcrq_replied		? 'R' : '-',
	    pcrq->pcrq_resend		? 'S' : '-',
	    pcrq->pcrq_restart		? 'T' : '-',
	    pcrq->pcrq_timedout		? 'X' : '-',
	    pcrq->pcrq_waiting		? 'W' : '-',
	    pcrq->pcrq_opc, abs(pcrq->pcrq_status),
	    pcrq->pcrq_peer,
	    pcrq->pcrq_request_portal, pcrq->pcrq_reply_portal,
	    pcrq->pcrq_reqlen, pcrq->pcrq_replen,
	    pcrq->pcrq_retries, pcrq->pcrq_nwaiters);

	pflctl_print_extra_field(&remaining, " %4d",
	    pcrq->pcrq_nob_received);
	pflctl_print_extra_field(&remaining, " %2"PRId64,
	    pcrq->pcrq_transno);
	pflctl_print_extra_field(&remaining, " %2d",
	    pcrq->pcrq_import_generation);

	(void)printf("\n");
}

int
psc_ctlmsg_rpcsvc_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	printf("%-10s %3s "
	    "%4s %4s %5s "
	    "%5s %4s %4s "
	    "%4s %4s %4s "
	    "%5s %6s %5s\n",
	    "rpcsvc", "flg",
	    "rqsz", "rpsz", "bufsz",
	    "#bufs", "qptl", "pptl",
	    "#thr", "#que", "#act",
	    "#wait", "#outrp", "nrqbd");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
psc_ctlmsg_rpcsvc_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct psc_ctlmsg_rpcsvc *pcrs = m;

	printf("%-10s   %c "
	    "%4d %4d %5d "
	    "%5d %4u %4u "
	    "%4d %4d %4d "
	    "%5d %6d %5d\n",
	    pcrs->pcrs_name,
	    pcrs->pcrs_flags & PSCRPC_SVCF_COUNT_PEER_QLENS ? 'Q' : '-',
	    pcrs->pcrs_rqsz, pcrs->pcrs_rpsz, pcrs->pcrs_bufsz,
	    pcrs->pcrs_nbufs, pcrs->pcrs_rqptl, pcrs->pcrs_rpptl,
	    pcrs->pcrs_nthr, pcrs->pcrs_nque, pcrs->pcrs_nact,
	    pcrs->pcrs_nwq, pcrs->pcrs_nrep, pcrs->pcrs_nrqbd);
}

int
pfl_ctlmsg_fsrq_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	(void)printf("%-16s %-4s %-9s %1s %-6s %1s %5s "
	    "%6s %-10s %4s\n",
	    "fsrq-address", "thr", "op", "f", "module", "r", "euid",
	    "pid", "start", "rc");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
pfl_ctlmsg_fsrq_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct pfl_ctlmsg_fsrq *pcfr = m;

	(void)printf("%016"PRIx64" %-4s %-9s "
	    "%c "
	    "%-6s %1d %5d "
	    "%6d %10"PRIu64" %4d\n",
	    pcfr->pcfr_req, pcfr->pcfr_thread + 5, pcfr->pcfr_opname,
	    pcfr->pcfr_flags & PFLCTL_FSRQF_INTR ? 'I' : '-',
	    pcfr->pcfr_mod, pcfr->pcfr_refcnt, pcfr->pcfr_euid,
	    pcfr->pcfr_pid, pcfr->pcfr_start.tv_sec, pcfr->pcfr_rc);
}

int
pfl_ctlmsg_workrq_prhdr(__unusedx struct psc_ctlmsghdr *mh,
    __unusedx const void *m)
{
	(void)printf("%-16s %-30s\n", "workrq-address", "type");
	return(PSC_CTL_DISPLAY_WIDTH);
}

void
pfl_ctlmsg_workrq_prdat(__unusedx const struct psc_ctlmsghdr *mh,
    const void *m)
{
	const struct pfl_ctlmsg_workrq *pcw = m;

	(void)printf("%016"PRIx64" %-30s\n", pcw->pcw_addr,
	    pcw->pcw_type);
}

__static void
psc_ctlmsg_print(struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_prfmt *prf;
	int i, len;

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
		i = prf->prf_check(mh, m);
		if (i == -1)
			return;
		else if (i)
			psc_fatalx("invalid ctlmsg size; type=%d sizeof=%zu "
			    "expected=%d", mh->mh_type, mh->mh_size, i);
	}

	/* Print display header. */
	if (!psc_ctl_noheader && psc_ctl_lastmsgtype != mh->mh_type &&
	    prf->prf_prhdr != NULL) {

		if (psc_ctl_lastmsgtype != -1)
			printf("\n");

		len = prf->prf_prhdr(mh, m);
		// psc_assert(len >= PSC_CTL_DISPLAY_WIDTH);

		for (i = 0; i < len; i++)
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
		pscthr_yield();
	}
	if (n == -1)
		err(1, "read");
	psc_free(m, 0);

	spinlock(&psc_ctl_lock);
	s = psc_ctl_sock;
	psc_ctl_sock = -1;
	freelock(&psc_ctl_lock);

	close(s);
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

int psc_ctlcli_docurses = 1;
struct psc_ctlcli_main_args {
	const char		 *osockfn;
	int			  ac;
	char			**av;
	const struct psc_ctlopt	 *otab;
	int			  notab;
} psc_ctlcli_main_args;

int psc_ctlcli_retry_main;
int psc_ctlcli_retry_fd;

void
psc_ctlcli_retry(void)
{
	struct psc_ctlcli_main_args *a = &psc_ctlcli_main_args;

	if (psc_ctlcli_retry_main) {
		char fn[PATH_MAX];

		psc_ctlcli_retry_main = 0;
		psc_ctlcli_docurses = 0;

		snprintf(fn, sizeof(fn), "/dev/fd/%d",
		    psc_ctlcli_retry_fd);
		(void)freopen(fn, "w", stderr);

		nofilter();
		endwin();

		psc_ctlcli_main(a->osockfn, a->ac, a->av, a->otab,
		    a->notab);
	}
}

void
psc_ctlcli_main(const char *osockfn, int ac, char *av[],
    const struct psc_ctlopt *otab, int notab)
{
	extern const char *daemon_name, *__progname;
	char optstr[LINE_MAX], chbuf[3], *p;
	struct sockaddr_un saun;
	struct psc_thread *thr;
	const char *prg;
	pthread_t pthr;
	int rc, c, i;

	if (psc_ctlcli_docurses && isatty(STDOUT_FILENO)) {
		FILE *fp;

		filter();

		/*
		 * Hack to work around initscr() from exiting the entire
		 * program.  We register an exit handler that actually
		 * calls this same function again skipping the curses
		 * routines.
		 */
		atexit(psc_ctlcli_retry);
		psc_ctlcli_main_args.osockfn = osockfn;
		psc_ctlcli_main_args.ac = ac;
		psc_ctlcli_main_args.av = av;
		psc_ctlcli_main_args.otab = otab;
		psc_ctlcli_main_args.notab = notab;

		psc_ctlcli_retry_fd = dup(fileno(stderr));

		/* Temporarily discard error output. */
		(void)freopen(_PATH_DEVNULL, "w", stderr);

		psc_ctlcli_retry_main = 1;
		initscr();
		psc_ctlcli_retry_main = 0;

		fclose(stderr);
		fp = fdopen(psc_ctlcli_retry_fd, "w");
		memcpy(stderr, fp, sizeof(*fp));

		start_color();
		psc_ctl_hascolors = has_colors();
		endwin();
	}

	prg = strrchr(__progname, '/');
	if (prg)
		prg++;
	else
		prg = __progname;

	pscthr_init(PCTHRT_WR, NULL, 0, "%swrthr", prg);

	psc_ctl_sockfn = osockfn;

	/* will be overwritten shortly if -S is used */
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
		/*
		 * Special shortcut case for version reporting.  This
		 * must be handled before connecting to the socket so it
		 * always work.  Note that other actions need the socket
		 * to work.
		 */
		if (c == 'V') {
			void (*cbf)(void);

			cbf = otab[i].pco_data;
			cbf();
			exit(0);
		}
	}

	/* Connect to control socket. */
	if ((psc_ctl_sock = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	memset(&saun, 0, sizeof(saun));
	saun.sun_family = AF_LOCAL;
	SOCKADDR_SETLEN(&saun);

	(void)FMTSTR(saun.sun_path, sizeof(saun.sun_path), psc_ctl_sockfn,
	    FMTSTRCASE('h', "s", psc_hostshort)
	    FMTSTRCASE('n', "s", daemon_name)
	);

	if (connect(psc_ctl_sock, (struct sockaddr *)&saun,
	    sizeof(saun)) == -1)
		err(1, "connect: %s", saun.sun_path);

	thr = pscthr_init(PCTHRT_RD, psc_ctlcli_rd_main, 1, "%srdthr", prg);
	pthr = thr->pscthr_pthread;
	pscthr_setready(thr);

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
				((void (*)(const char *))otab[
				    i].pco_data)(optarg);
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
		errx(1, "no actions specified.");

	psc_ctlmsg_sendlast();

	spinlock(&psc_ctl_lock);
	if (psc_ctl_sock != -1 && shutdown(psc_ctl_sock, SHUT_WR) == -1)
		psc_fatal("shutdown");
	freelock(&psc_ctl_lock);

	rc = pthread_join(pthr, NULL);
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

void
psc_ctl_prnumber(int base10, uint64_t n, int width, const char *suf)
{
	int col = -1;

	if (base10) {
		if (n < 10)
			col = COLOR_RED;
		else if (n < 100)
			col = COLOR_YELLOW;
		else if (n < 1000)
			col = COLOR_GREEN;
		else if (n < 10000)
			col = COLOR_BLUE;
		else
			col = COLOR_MAGENTA;

	} else {
		if (n < 1024)
			col = COLOR_RED;
		else if (n < 1024 * 1024)
			col = COLOR_YELLOW;
		else if (n < 1024 * 1024 * 1024)
			col = COLOR_GREEN;
		else if (n < UINT64_C(1024) * 1024 * 1024 * 1024)
			col = COLOR_BLUE;
		else
			col = COLOR_MAGENTA;
	}
	if (n)
		setcolor(col);
	if (psc_ctl_inhuman || base10)
		printf("%*"PRIu64, width ? width : 7, n);
	else {
		char buf[PSCFMT_HUMAN_BUFSIZ];

		pfl_fmt_human(buf, n);
		printf("%*s", width, buf);
	}
	if (suf)
		printf("%s", suf);
	if (n)
		uncolor();
}
