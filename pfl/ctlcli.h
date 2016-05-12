/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

/*
 * This file contains common definitions among client programs that
 * connect to daemons to issue control operations.
 */

#ifndef _PFL_CTLCLI_H_
#define _PFL_CTLCLI_H_

struct psc_ctlmsg_thread;
struct psc_ctlmsghdr;

#define PSC_CTL_DISPLAY_WIDTH	80

/* Must be kept in sync with the PCMT_* list. */
#define PSC_CTLMSG_PRFMT_DEFS															\
	{ NULL /* ERROR */,		psc_ctlmsg_error_prdat,		sizeof(struct psc_ctlmsg_error),	NULL },				\
	{ psc_ctlmsg_fault_prhdr,	psc_ctlmsg_fault_prdat,		sizeof(struct psc_ctlmsg_fault),	NULL },				\
	{ pfl_ctlmsg_fsrq_prhdr,	pfl_ctlmsg_fsrq_prdat,		sizeof(struct pfl_ctlmsg_fsrq),		NULL },				\
	{ psc_ctlmsg_hashtable_prhdr,	psc_ctlmsg_hashtable_prdat,	sizeof(struct psc_ctlmsg_hashtable),	NULL },				\
	{ psc_ctlmsg_journal_prhdr,	psc_ctlmsg_journal_prdat,	sizeof(struct psc_ctlmsg_journal),	NULL },				\
	{ psc_ctlmsg_listcache_prhdr,	psc_ctlmsg_listcache_prdat,	sizeof(struct psc_ctlmsg_listcache),	NULL },				\
	{ psc_ctlmsg_lnetif_prhdr,	psc_ctlmsg_lnetif_prdat,	sizeof(struct psc_ctlmsg_lnetif),	NULL },				\
	{ psc_ctlmsg_meter_prhdr,	psc_ctlmsg_meter_prdat,		sizeof(struct psc_ctlmsg_meter),	NULL },				\
	{ psc_ctlmsg_mlist_prhdr,	psc_ctlmsg_mlist_prdat,		sizeof(struct psc_ctlmsg_mlist),	NULL },				\
	{ psc_ctlmsg_odtable_prhdr,	psc_ctlmsg_odtable_prdat,	sizeof(struct psc_ctlmsg_odtable),	NULL },				\
	{ psc_ctlmsg_opstat_prhdr,	psc_ctlmsg_opstat_prdat,	sizeof(struct psc_ctlmsg_opstat),	NULL },				\
	{ psc_ctlmsg_param_prhdr,	psc_ctlmsg_param_prdat,		sizeof(struct psc_ctlmsg_param),	NULL },				\
	{ psc_ctlmsg_pool_prhdr,	psc_ctlmsg_pool_prdat,		sizeof(struct psc_ctlmsg_pool),		NULL },				\
	{ psc_ctlmsg_rpcrq_prhdr,	psc_ctlmsg_rpcrq_prdat,		sizeof(struct psc_ctlmsg_rpcrq),	NULL },				\
	{ psc_ctlmsg_rpcsvc_prhdr,	psc_ctlmsg_rpcsvc_prdat,	sizeof(struct psc_ctlmsg_rpcsvc),	NULL },				\
	{ NULL /* GETSUBSYS */,		NULL,				0,					psc_ctlmsg_subsys_check },	\
	{ psc_ctlmsg_thread_prhdr,	psc_ctlmsg_thread_prdat,	0,					psc_ctlmsg_thread_check },	\
	{ pfl_ctlmsg_workrq_prhdr,	pfl_ctlmsg_workrq_prdat,	sizeof(struct pfl_ctlmsg_workrq),	NULL },				\
	{ NULL /* SETPARAM */,		NULL,				0,					NULL }

#define PSC_CTLSHOW_DEFS						\
	{ "faults",		psc_ctl_packshow_fault },		\
	{ "fsrq",		pfl_ctl_packshow_fsrq },		\
	{ "hashtables",		psc_ctl_packshow_hashtable },		\
	{ "journals",		psc_ctl_packshow_journal },		\
	{ "listcaches",		psc_ctl_packshow_listcache },		\
	{ "lnetif",		psc_ctl_packshow_lnetif },		\
	{ "meters",		psc_ctl_packshow_meter },		\
	{ "mlists",		psc_ctl_packshow_mlist },		\
	{ "odtables",		psc_ctl_packshow_odtable },		\
	{ "opstats",		psc_ctl_packshow_opstat },		\
	{ "pools",		psc_ctl_packshow_pool },		\
	{ "rpcrqs",		psc_ctl_packshow_rpcrq },		\
	{ "rpcsvcs",		psc_ctl_packshow_rpcsvc },		\
	{ "threads",		psc_ctl_packshow_thread },		\
	{ "workrq",		pfl_ctl_packshow_workrq }

struct psc_ctlshow_ent {
	const char		 *pse_name;
	void			(*pse_cb)(char *);
};

struct psc_ctlcmd_req {
	const char		 *pccr_name;
	void			(*pccr_cbf)(int, char **);
};

struct psc_ctlmsg_prfmt {
	int			(*prf_prhdr)(struct psc_ctlmsghdr *, const void *);
	void			(*prf_prdat)(const struct psc_ctlmsghdr *, const void *);
	size_t			  prf_msgsiz;
	int			(*prf_check)(struct psc_ctlmsghdr *, const void *);
};

enum psc_ctlopt_type {
	PCOF_FLAG,
	PCOF_FUNC
};

struct psc_ctlopt {
	char			 pco_ch;
	enum psc_ctlopt_type	 pco_type;
	void			*pco_data;
};

void  psc_ctlparse_lc(char *);
void  psc_ctlparse_param(char *);
void  psc_ctlparse_pool(char *);
void  psc_ctlparse_show(char *);

void  psc_ctl_packshow_fault(char *);
void  pfl_ctl_packshow_fsrq(char *);
void  psc_ctl_packshow_hashtable(char *);
void  psc_ctl_packshow_journal(char *);
void  psc_ctl_packshow_listcache(char *);
void  psc_ctl_packshow_lnetif(char *);
void  psc_ctl_packshow_meter(char *);
void  psc_ctl_packshow_mlist(char *);
void  psc_ctl_packshow_odtable(char *);
void  psc_ctl_packshow_opstat(char *);
void  psc_ctl_packshow_pool(char *);
void  psc_ctl_packshow_rpcrq(char *);
void  psc_ctl_packshow_rpcsvc(char *);
void  psc_ctl_packshow_thread(char *);
void  pfl_ctl_packshow_workrq(char *);

void *psc_ctlmsg_push(int, size_t);
void  psc_ctlcli_main(const char *, int, char **, const struct psc_ctlopt *, int);

void  psc_ctlmsg_error_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_fault_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_fault_prhdr(struct psc_ctlmsghdr *, const void *);
void  pfl_ctlmsg_fsrq_prdat(const struct psc_ctlmsghdr *, const void *);
int   pfl_ctlmsg_fsrq_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_hashtable_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_hashtable_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_opstat_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_opstat_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_journal_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_journal_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_lnetif_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_lnetif_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_listcache_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_listcache_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_meter_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_meter_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_mlist_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_mlist_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_odtable_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_odtable_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_param_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_param_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_pool_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_pool_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_rpcrq_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_rpcrq_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_rpcsvc_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_rpcsvc_prhdr(struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_thread_check(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_thread_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_thread_prhdr(struct psc_ctlmsghdr *, const void *);
void  pfl_ctlmsg_workrq_prdat(const struct psc_ctlmsghdr *, const void *);
int   pfl_ctlmsg_workrq_prhdr(struct psc_ctlmsghdr *, const void *);

int   psc_ctl_get_display_maxwidth(void);
void  psc_ctl_prnumber(int, uint64_t, int, const char *);

void  setcolor(int);
void  uncolor(void);

extern int psc_ctl_inhuman;
extern int psc_ctl_lastmsgtype;
extern int psc_ctl_nodns;
extern int psc_ctl_noheader;

extern struct psc_ctlshow_ent	 psc_ctlshow_tab[];
extern int			 psc_ctlshow_ntabents;
extern struct psc_ctlmsg_prfmt	 psc_ctlmsg_prfmts[];
extern int			 psc_ctlmsg_nprfmts;
extern struct psc_ctlcmd_req	 psc_ctlcmd_reqs[];
extern int			 psc_ctlcmd_nreqs;

#define PFLCTL_CLI_DEFS							\
int psc_ctlshow_ntabents = nitems(psc_ctlshow_tab);			\
int psc_ctlmsg_nprfmts = nitems(psc_ctlmsg_prfmts);			\
int psc_ctlcmd_nreqs = nitems(psc_ctlcmd_reqs)

#endif /* _PFL_CTLCLI_H_ */
