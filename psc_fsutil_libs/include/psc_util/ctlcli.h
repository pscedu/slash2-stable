/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

struct psc_ctlmsg_stats;
struct psc_ctlmsghdr;

#define PSC_CTL_DISPLAY_WIDTH 80

#define PSC_CTLMSG_PRFMT_DEFS															\
/* 0 */	{ NULL,				psc_ctlmsg_error_prdat,		sizeof(struct psc_ctlmsg_error),	NULL },				\
/* 1 */	{ psc_ctlmsg_loglevel_prhdr,	psc_ctlmsg_loglevel_prdat,	0,					psc_ctlmsg_loglevel_check },	\
/* 2 */	{ psc_ctlmsg_lc_prhdr,		psc_ctlmsg_lc_prdat,		sizeof(struct psc_ctlmsg_lc),		NULL },				\
/* 3 */	{ psc_ctlmsg_stats_prhdr,	psc_ctlmsg_stats_prdat,		sizeof(struct psc_ctlmsg_stats),	NULL },				\
/* 4 */	{ NULL,				NULL,				0,					psc_ctlmsg_subsys_check },	\
/* 5 */	{ psc_ctlmsg_hashtable_prhdr,	psc_ctlmsg_hashtable_prdat,	sizeof(struct psc_ctlmsg_hashtable),	NULL },				\
/* 6 */	{ psc_ctlmsg_param_prhdr,	psc_ctlmsg_param_prdat,		sizeof(struct psc_ctlmsg_param),	NULL },				\
/* 7 */	{ NULL,				NULL,				0,					NULL },				\
/* 8 */	{ psc_ctlmsg_iostats_prhdr,	psc_ctlmsg_iostats_prdat,	sizeof(struct psc_ctlmsg_iostats),	NULL },				\
/* 9 */	{ psc_ctlmsg_meter_prhdr,	psc_ctlmsg_meter_prdat,		sizeof(struct psc_ctlmsg_meter),	NULL },				\
/*10 */	{ psc_ctlmsg_pool_prhdr,	psc_ctlmsg_pool_prdat,		sizeof(struct psc_ctlmsg_pool),		NULL },				\
/*11 */	{ psc_ctlmsg_mlist_prhdr,	psc_ctlmsg_mlist_prdat,		sizeof(struct psc_ctlmsg_mlist),	NULL },				\
/*12 */	{ psc_ctlmsg_fault_prhdr,	psc_ctlmsg_fault_prdat,		sizeof(struct psc_ctlmsg_fault),	NULL },				\
/*13 */	{ NULL,				NULL,				0,					NULL }

struct psc_ctlshow_ent {
	const char	 *pse_name;
	void		(*pse_cb)(const char *);
};

struct psc_ctlcmd_req {
	const char *	  pccr_name;
	int		  pccr_opcode;
};

struct psc_ctl_thrstatfmt {
	void		(*ptf_prdat)(const struct psc_ctlmsg_stats *);
};

struct psc_ctlmsg_prfmt {
	void		(*prf_prhdr)(struct psc_ctlmsghdr *, const void *);
	void		(*prf_prdat)(const struct psc_ctlmsghdr *, const void *);
	size_t		  prf_msgsiz;
	int		(*prf_check)(struct psc_ctlmsghdr *, const void *);
};

void  psc_ctlparse_cmd(char *);
void  psc_ctlparse_hashtable(const char *);
void  psc_ctlparse_iostats(char *);
void  psc_ctlparse_lc(char *);
void  psc_ctlparse_meter(char *);
void  psc_ctlparse_mlist(char *);
void  psc_ctlparse_param(char *);
void  psc_ctlparse_pool(char *);
void  psc_ctlparse_show(char *);

void  psc_ctl_packshow_loglevel(const char *);
void  psc_ctl_packshow_stats(const char *);
void  psc_ctl_packshow_faults(const char *);

void *psc_ctlmsg_push(int, size_t);
void  psc_ctlcli_main(const char *);

void  psc_ctlthr_prdat(const struct psc_ctlmsg_stats *);

void  psc_ctlmsg_hashtable_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_hashtable_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_error_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_iostats_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_iostats_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_meter_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_meter_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_pool_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_pool_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_lc_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_lc_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_param_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_param_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_stats_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_stats_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_loglevel_check(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_loglevel_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_loglevel_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_mlist_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_mlist_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_fault_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_fault_prdat(const struct psc_ctlmsghdr *, const void *);

extern int psc_ctl_noheader;
extern int psc_ctl_inhuman;
extern int psc_ctl_lastmsgtype;

extern struct psc_ctl_thrstatfmt psc_ctl_thrstatfmts[];
extern int			 psc_ctl_nthrstatfmts;
extern struct psc_ctlshow_ent	 psc_ctlshow_tab[];
extern int			 psc_ctlshow_ntabents;
extern struct psc_ctlmsg_prfmt	 psc_ctlmsg_prfmts[];
extern int			 psc_ctlmsg_nprfmts;
extern struct psc_ctlcmd_req	 psc_ctlcmd_reqs[];
extern int			 psc_ctlcmd_nreqs;
