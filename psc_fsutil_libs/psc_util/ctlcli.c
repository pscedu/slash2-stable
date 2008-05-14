/* $Id$ */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "psc_ds/list.h"
#include "psc_util/log.h"
#include "../slashd/control.h"
#include "psc_util/subsys.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/cdefs.h"

__static PSCLIST_HEAD(psc_ctlmsgs);

struct psc_ctlmsg {
	struct psclist_head	pcm_lentry;
	struct psc_ctlmsghdr	pcm_mh;
};

int psc_ctl_noheader;
int psc_ctl_inhuman;
int psc_ctl_nsubsys;
char *psc_ctl_subsys_names;

struct psc_ctlshow_ent {
	const char	*pse_name;
	int		 pse_value;
};

__static int
psc_ctlshow_lookup(const char *name)
{
	extern psc_ctlshow_ent psc_ctlshow_tab[];
	int n;

	if (strlen(name) == 0)
		return (-1);
	for (n = 0; psc_ctlshow_tab[n].psc_name; n++)
		if (strncasecmp(name, psc_ctlshow_tab[n].s_name,
		    strlen(name)) == 0)
			return (psc_ctlshow_tab[n].s_value);
	return (-1);
}

void *
psc_ctlmsg_push(int type, size_t msiz)
{
	struct psc_ctlmsg *pcm;
	static int id;
	size_t tsiz;

	tsiz = msiz + sizeof(*pcm);
	pcm = PSCALLOC(tsiz);
	psclist_xadd_tail(&pcm->pcm_lentry, &psc_ctlmsgs);
	pcm->pcm_mh.mh_type = type;
	pcm->pcm_mh.mh_size = msiz;
	pcm->pcm_mh.mh_id = id++;
	return (&pcm->pcm.mh_data);
}

void
psc_ctlparse_hashtable(const char *tblname)
{
	struct psc_ctlmsg_hashtable *pcht;

	pcht = pushmsg(PCMT_GETHASHTABLE, sizeof(*pcht));
	snprintf(pcht->pcht_name, sizeof(pcht->pcht_name), "%s", tblname);
}

void
psc_ctlparse_show(char *showspec)
{
	char *thrlist, *thr, *thrnext;
	struct psc_ctlmsg_loglevel *pcl;
	struct psc_ctlmsg_subsys *pcss;
	struct psc_ctlmsg_stats *pcst;
	int n, type;

	if ((thrlist = strchr(showspec, ':')) == NULL)
		thrlist = PCTHRNAME_EVERYONE;
	else
		*thrlist++ = '\0';

	if ((type = psc_ctlshow_lookup(showspec)) == -1)
		psc_fatalx("invalid show parameter: %s", showspec);

	for (thr = thrlist; thr != NULL; thr = thrnext) {
		if ((thrnext = strchr(thr, ',')) != NULL)
			*thrnext++ = '\0';

		switch (type) {
		case PCMT_GETLOGLEVEL:
			pcss = pushmsg(PCMT_GETSUBSYS, sizeof(*pcss));

			pcl = pushmsg(type, sizeof(*pcl));
			n = snprintf(pcl->pcl_thrname,
			   sizeof(pcl->pcl_thrname), "%s", thr);
			if (n == -1)
				psc_fatal("snprintf");
			else if (n == 0 ||
			    n > (int)sizeof(pcl->pcl_thrname))
				psc_fatalx("invalid thread name: %s", thr);
			break;
		case PCMT_GETSTATS:
			pcst = pushmsg(type, sizeof(*pcst));
			n = snprintf(pcst->pcst_thrname,
			   sizeof(pcst->pcst_thrname), "%s", thr);
			if (n == -1)
				psc_fatal("snprintf");
			else if (n == 0 ||
			    n > (int)sizeof(pcst->pcst_thrname))
				psc_fatalx("invalid thread name: %s", thr);
			break;
		}
	}
}

void
psc_ctlparse_lc(char *lists)
{
	struct psc_ctlmsg_lc *pclc;
	char *list, *listnext;
	int n;

	for (list = lists; list != NULL; list = listnext) {
		if ((listnext = strchr(list, ',')) != NULL)
			*listnext++ = '\0';

		pclc = pushmsg(PCMT_GETLC, sizeof(*pclc));

		n = snprintf(pclc->pclc_name, sizeof(pclc->pclc_name),
		    "%s", list);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pclc->pclc_name))
			psc_fatalx("invalid list: %s", list);
	}
}

void
psc_ctlparse_param(char *spec)
{
	struct psc_ctlmsg_param *pcp;
	char *thr, *field, *value;
	int n;

	if ((value = strchr(spec, '=')) != NULL)
		*value++ = '\0';

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
		psc_fatalx("invalid thread name: %s", thr);

	/* Set parameter name. */
	n = snprintf(pcp->pcp_field, sizeof(pcp->pcp_field),
	    "%s", field);
	if (n == -1)
		psc_fatal("snprintf");
	else if (n == 0 || n > (int)sizeof(pcp->pcp_field))
		psc_fatalx("invalid parameter: %s", thr);

	/* Set parameter value (if applicable). */
	if (value) {
		n = snprintf(pcp->pcp_value,
		    sizeof(pcp->pcp_value), "%s", value);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pcp->pcp_value))
			psc_fatalx("invalid parameter value: %s", thr);
	}
}

void
psc_ctlparse_iostat(char *iostats)
{
	struct psc_ctlmsg_iostats *pci;
	char *iostat, *next;
	int n;

	for (iostat = iostats; iostat != NULL; iostat = next) {
		if ((next = strchr(iostat, ',')) != NULL)
			*next++ = '\0';

		pci = pushmsg(PCMT_GETIOSTATS, sizeof(*pci));

		/* Set iostat name. */
		n = snprintf(pci->pci_ist.ist_name,
		    sizeof(pci->pci_ist.ist_name), "%s", iostat);
		if (n == -1)
			psc_fatal("snprintf");
		else if (n == 0 || n > (int)sizeof(pci->pci_ist.ist_name))
			psc_fatalx("invalid iostat name: %s", iostat);
	}
}

int
psc_ctl_loglevel_namelen(int n)
{
	size_t maxlen;
	int j;

	maxlen = strlen(&subsys_names[n * PCSS_NAME_MAX]);
	for (j = 0; j < PNLOGLEVELS + 1; j++)
		maxlen = MAX(maxlen, strlen(psclog_name(j)));
	return (maxlen);
}

void
psc_humanscale(char buf[8], double num)
{
	int mag;

	/*
	 * 1234567
	 * 1000.3K
	 */
	for (mag = 0; num > 1024.0; mag++)
		num /= 1024.0;
	if (mag > 6)
		snprintf(buf, sizeof(buf), "%.1e", num);
	else
		snprintf(buf, sizeof(buf), "%6.1f%c", num, "BKMGTPE"[mag]);
}

int
psc_ctlthr_prhdr(void)
{
	return (printf(" %-*s %8s", PSCTHR_NAME_MAX,
	    "#nclients", "#sent", "#recv");
								                                     "thread", "#clients");
}

void
psc_ctlthr_prdat(const struct psc_ctlmsg_stat *pcst)
{
	printf(" %-*s %8u\n", PSCTHR_NAME_MAX,
	    pcst->pcst_thrname, pcst->pcst_nclients,
	    pcst->pcst_nsent, pcst->pcst_nrecv);
}

struct psc_thrstatfmt {
	int  (*ptf_prhdr)(void);
	void (*ptf_prdat)(const struct psc_ctlmsg_stat *);
} psc_thrstatfmt[] = {
	{ psc_ctlthr_prhdr, psc_ctlthr_prdat }
};


struct psc_ctlmsg_prfmt {
	int	(*prf_prhdr)(void);
	void	(*prf_prdat)(const void *)
	size_t	  prf_msgsiz;
	int	(*prf_check)(const struct psc_ctlmsghdr *);
};

int
psc_ctlmsg_hashtable_prhdr(void)
{
	printf("hash table statistics\n");
	return (printf("%12s %6s %6s %7s %6s %6s %6s\n",
	    "table", "total", "used", "%use", "ents",
	    "avglen", "maxlen"));
}

void
psc_ctlmsg_hashtable_prdat(const void *m)
{
	const struct psc_ctlmsg_hashtable *pcht = m;

	printf("%12s %6d %6d %6.2f%% %6d %6.1f %6d\n",
	    pcht->pcht_name,
	    pcht->pcht_totalbucks, pcht->pcht_usedbucks,
	    pcht->pcht_usedbucks * 100.0 / pcht->pcht_totalbucks,
	    pcht->pcht_nents,
	    pcht->pcht_nents * 1.0 / pcht->pcht_totalbucks,
	    pcht->pcht_maxbucklen);
}

void
psc_ctlmsg_error_prdat(const void *m)
{
	const struct psc_ctlmsg_error *pce = m;

	printf("error: %s\n", pce->pce_errmsg);
}

int
psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *mh)
{
	if (mh->mh_size == 0 ||
	    mh->mh_size % PCSS_NAME_MAX)
		return (sizeof(struct psc_ctlmsg_subsys));
	psc_ctl_nsubsys = mh->mh_size / PCSS_NAME_MAX;
	psc_ctl_subsys_names = PSCALLOC(mh->mh_size);
	memcpy(psc_ctl_subsys_names, pcss->pcss_names, mh->mh_size);
	return (0);
}

int
psc_ctlmsg_iostats_prhdr(void)
{
	printf("iostats\n");
	return (printf(" %-12s %9s %8s %8s %8s\n",
	    "name", "ratecur", "total", "erate", "toterr"));
}

void
psc_ctlmsg_iostats_prdat(const void *m)
{
	const struct psc_ctlmsg_iostats *pci = m;
	const struct ist = &pci->pci_ist;

	printf(" %-12s ", ist->ist_name);
	if (psc_ctl_inhuman) {
		printf("%8.2f ", ist->ist_rate);
		printf("%8"_P_LP64"u ", ist->ist_bytes_total);
	} else {
		psc_humanscale(buf, ist->ist_rate);
		printf("%7s/s ", buf);

		psc_humanscale(buf, ist->ist_bytes_total);
		printf("%8s ", buf);
	}
	printf("%6.1f/s %8"_P_LP64"u\n", ist->ist_erate,
	    ist->ist_errors_total);
}

int
psc_ctlmsg_lc_prhdr(void)
{
	printf("list caches\n");
	return (printf(" %20s %8s %9s %8s\n",
	    "list", "size", "max", "#seen"));
}

void
psc_ctlmsg_lc_prdat(const void *m)
{
	const struct psc_ctlmsg_lc *pclc = m;

	printf(" %20s %8zu ", pclc->pclc_name, pclc->pclc_size);
	if (pclc->pclc_max == (size_t)-1)
		printf(" %9s", "unlimited");
	else
		printf(" %9zu", pclc->pclc_max);
	printf("%8zu\n", pclc->pclc_nseen);
}

int
psc_ctlmsg_param_prhdr(void)
{
	printf("parameters\n");
	return (printf(" %-30s %s\n", "name", "value"));
}

void
psc_ctlmsg_param_prdat(const void *m)
{
	if (strcmp(pcp->pcp_thrname, PCTHRNAME_EVERYONE) == 0)
		printf(" %-30s %s\n", pcp->pcp_field, pcp->pcp_value);
	else
		printf(" %s.%s %s\n", pcp->pcp_thrname,
		    pcp->pcp_field, pcp->pcp_value);
}



	static int last_thrtype = -1;

		pcst = m;
		if (pcst->pcst_thrtype >= 0 &&
		    pcst->pcst_thrtype < nthrstatf &&
		    psc_thrstatfmt[pcst->pcst_thrtype] == NULL)
			break;

		if (lastmsgtype != PCMT_GETSTATS) {
			printf("thread stats\n");
			last_thrtype = -1;
		}

		if (pcst->pcst_thrtype < 0 ||
		    pcst->pcst_thrtype >= nthrstatf) {
			psc_warnx("invalid thread type: %d",
			    pcst->pcst_thrtype);
			break;
		}

		/* print subheader for each thread type */
		if (last_thrtype != pcst->pcst_thrtype) {
			if (lastmsgtype == PCMT_GETSTATS)
				printf("\n");
			len = psc_thrstatfmt[pcst->pcst_thrtype].ptf_prhdr();
			putchar('\n');
			for (n = 0; n < len; n++)
				putchar('=');
			putchar('\n');
		}
		last_thrtype = pcst->pcst_thrtype;

		/* print thread stats */
		psc_thrstatfmt[pcst->pcst_thrtype].ptf_prdat(pcst);



int
psc_ctlmsg_loglevel_check(const struct psc_ctlmsghdr *mh)
{
	__unusedx struct psc_ctlmsg_loglevel *pcl;

	if (mh->mh_size != sizeof(*pcl) +
	    psc_ctl_nsubsys * sizeof(*pcl->pcl_levels))
		return (sizeof(*pcl));
	return (0);
}

int
psc_ctlmsg_loglevel_prhdr(void)
{
	int len, n;

	printf("logging levels\n");
	len = printf(" %-*s ", PSCTHR_NAME_MAX, "thread");
	for (n = 0; n < psc_ctl_nsubsys; n++)
		len += printf(" %*s", psc_ctl_loglevel_namelen(n),
		    &psc_ctl_subsys_names[n * PCSS_NAME_MAX]);
	return (len);
}

void
psc_ctlmsg_loglevel_prdat(const void *m)
{
	struct psc_ctlmsg_loglevel *pcl = m;
	int n;

	printf(" %-*s ", PSCTHR_NAME_MAX, pcl->pcl_thrname);
	for (n = 0; n < psc_ctl_nsubsys; n++)
		printf(" %*s", psc_ctl_loglevel_namelen(n),
		    psclog_name(pcl->pcl_levels[n]));
	printf("\n");
}

__static void
psc_ctlmsg_print(const struct psc_ctlmsghdr *mh, const void *m)
{
	const struct psc_ctlmsg_prfmt *prf;
	static int lastmsgtype = -1;
	int n, len;

	/* Validate message type. */
	if (mh->mh_type < 0 ||
	    mh->mh_type >= NENTRIES(psc_ctlmsg_prfmts))
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	prf = &psc_ctlmsg_prfmts[mh->mh_type];

	/* Validate message size. */
	if (prf->prf_siz) {
		if (prf->prf_siz != mh.mh_size)
			psc_fatalx("invalid ctlmsg size; type=%d; "
			    "sizeof=%zu expected=%zu", mh->mh_type,
			    mh->mh_size, prf->prf_siz);
	} else if (prf->prf_check == NULL) {
		/* Disallowed message type. */
		psc_fatalx("invalid ctlmsg type %d", mh->mh_type);
	} else if ((n = prf->prf_check(mh)) != 0) {
		psc_fatalx("invalid ctlmsg size; type=%d; sizeof=%zu "
		    "expected=%zu", mh->mh_type, mh->mh_size, n);

	/* Print display header. */
	if (!noheader && lastmsgtype != mh->mh_type &&
	    prf->prf_prhdr != NULL) {
		if (lastmsgtype != -1)
			printf("\n");
		len = prf->prf_prhdr() - 1;
		for (n = 0; n < len; n++)
			putchar('=');
		putchar('\n');
	}

	/* Print display contents. */
	if (prf->prf_prdat)
		prf->prf_prdat(m);
	lastmsgtype = mh->mh_type;
}

__dead void
psc_ctlcli_main(const char *sockfn)
{
	struct msg *msg, *nextm;
	struct sockaddr_un sun;
	ssize_t siz;
	int s;

	/* Connect to control socket. */
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		psc_fatal("socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", sockfn);
	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		psc_fatal("connect: %s", sockfn);

	/* Send queued control messages. */
	psclist_for_each_entry_safe(msg, mnext, &msgs, msg_link) {
		siz = msg->msg_mh.mh_size + sizeof(msg->msg_mh);
		if (write(s, &msg->msg_mh, siz) != siz)
			psc_fatal("write");
		free(msg);
	}
	if (shutdown(s, SHUT_WR) == -1)
		psc_fatal("shutdown");

	/* Read and print response messages. */
	m = NULL;
	siz = 0;
	while ((n = read(s, &mh, sizeof(mh))) != -1 && n != 0) {
		if (n != sizeof(mh)) {
			psc_warnx("short read");
			continue;
		}
		if (mh.mh_size == 0)
			psc_fatalx("received invalid message from daemon");
		if (mh.mh_size >= siz) {
			siz = mh.mh_size;
			if ((m = realloc(m, siz)) == NULL)
				psc_fatal("realloc");
		}
		n = read(s, m, mh.mh_size);
		if (n == -1)
			psc_fatal("read");
		else if (n == 0)
			psc_fatalx("received unexpected EOF from daemon");
		prm(&mh, m);
	}
	if (n == -1)
		psc_fatal("read");
	free(m);
	close(s);
}
