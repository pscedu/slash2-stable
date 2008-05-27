/* $Id$ */

#define PSCTHR_NAME_MAX 12

#define PSC_CTLMSG_PRFMT_DEFS															\
/* 0 */	{ NULL,				psc_ctlmsg_error_prdat,		sizeof(struct psc_ctlmsg_error),	NULL },				\
/* 1 */	{ psc_ctlmsg_loglevel_prhdr,	psc_ctlmsg_loglevel_prdat,	0,					psc_ctlmsg_loglevel_check },	\
/* 2 */	{ psc_ctlmsg_lc_prhdr,		psc_ctlmsg_lc_prdat,		sizeof(struct psc_ctlmsg_lc),		NULL },				\
/* 3 */	{ psc_ctlmsg_stats_prhdr,	psc_ctlmsg_stats_prdat,		sizeof(struct psc_ctlmsg_stats),	NULL },				\
/* 4 */	{ NULL,				NULL,				0,					psc_ctlmsg_subsys_check },	\
/* 5 */	{ psc_ctlmsg_hashtable_prhdr,	psc_ctlmsg_hashtable_prdat,	sizeof(struct psc_ctlmsg_hashtable),	NULL },				\
/* 6 */	{ psc_ctlmsg_param_prhdr,	psc_ctlmsg_param_prdat,		sizeof(struct psc_ctlmsg_param),	NULL },				\
/* 7 */	{ NULL,				NULL,				0,					NULL },				\
/* 8 */	{ psc_ctlmsg_iostats_prhdr,	psc_ctlmsg_iostats_prdat,	sizeof(struct psc_ctlmsg_iostats),	NULL }

struct psc_ctlshow_ent {
	const char	 *pse_name;
	void		(*pse_cb)(const char *);
};

struct psc_ctl_thrstatfmt {
	int		(*ptf_prhdr)(void);
	void		(*ptf_prdat)(const struct psc_ctlmsg_stats *);
};

struct psc_ctlmsg_prfmt {
	int		(*prf_prhdr)(struct psc_ctlmsghdr *, const void *);
	void		(*prf_prdat)(const struct psc_ctlmsghdr *, const void *);
	size_t		  prf_msgsiz;
	int		(*prf_check)(struct psc_ctlmsghdr *, const void *);
};

void  psc_ctlparse_hashtable(const char *);
void  psc_ctlparse_show(char *);
void  psc_ctlparse_lc(char *);
void  psc_ctlparse_param(char *);
void  psc_ctlparse_iostats(char *);

void  psc_ctl_packshow_loglevel(const char *);
void  psc_ctl_packshow_stats(const char *);

void *psc_ctlmsg_push(int, size_t);
void  psc_ctlcli_main(const char *);

int   psc_ctlthr_prhdr(void);
void  psc_ctlthr_prdat(const struct psc_ctlmsg_stats *);

int   psc_ctlmsg_hashtable_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_hashtable_prdat(const struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_error_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_subsys_check(struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_iostats_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_iostats_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_lc_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_lc_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_param_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_param_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_stats_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_stats_prdat(const struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_loglevel_check(struct psc_ctlmsghdr *, const void *);
int   psc_ctlmsg_loglevel_prhdr(struct psc_ctlmsghdr *, const void *);
void  psc_ctlmsg_loglevel_prdat(const struct psc_ctlmsghdr *, const void *);

extern int psc_ctl_noheader;
extern int psc_ctl_inhuman;

extern struct psc_ctl_thrstatfmt psc_ctl_thrstatfmts[];
extern int			 psc_ctl_nthrstatfmts;
extern struct psc_ctlshow_ent	 psc_ctlshow_tab[];
extern int			 psc_ctlshow_ntabents;
extern struct psc_ctlmsg_prfmt	 psc_ctlmsg_prfmts[];
extern int			 psc_ctlmsg_nprfmts;
