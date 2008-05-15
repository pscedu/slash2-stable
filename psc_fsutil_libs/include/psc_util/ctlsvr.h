/* $Id$ */

#include "psc_util/thread.h"

struct psc_ctlmsghdr;
struct psc_ctlmsg_param;

#define PSC_CTL_FOREACH_THREAD(i, thr, thrname, threads, nthreads)	\
	for ((i) = 0; ((thr) = (threads)[i]) && (i) < (nthreads); i++)	\
		if (strncmp((thr)->pscthr_name, (thrname),		\
		    strlen(thrname)) == 0 ||				\
		    strcmp((thrname), PCTHRNAME_EVERYONE) == 0)

#define psc_ctlthr(thr)	((struct psc_ctlthr *)(thr)->pscthr_private)

#define PSC_CTLDEFOPS								\
/* 0 */	{ NULL,				0 },					\
/* 1 */	{ psc_ctlrep_getloglevel,	sizeof(struct psc_ctlmsg_loglevel) },	\
/* 2 */	{ psc_ctlrep_getlc,		sizeof(struct psc_ctlmsg_lc) },		\
/* 3 */	{ psc_ctlrep_getstats,		sizeof(struct psc_ctlmsg_stats) },	\
/* 4 */	{ psc_ctlrep_getsubsys,		0 },					\
/* 5 */	{ psc_ctlrep_gethashtable,	sizeof(struct psc_ctlmsg_hashtable) },	\
/* 6 */	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
/* 7 */	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
/* 8 */	{ psc_ctlrep_getiostats,	sizeof(struct psc_ctlmsg_iostats) }

struct psc_ctlthr {
	int	  pc_st_nclients;
	int	  pc_st_nsent;
	int	  pc_st_nrecv;
};

struct psc_ctlop {
	void	(*pc_op)(int, struct psc_ctlmsghdr *, void *);
	size_t	  pc_siz;
};

void psc_ctlsenderr(int, struct psc_ctlmsghdr *, const char *, ...);

void psc_ctlrep_gethashtable(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_getiostats(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_getlc(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_getloglevel(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_getstats(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_getsubsys(int, struct psc_ctlmsghdr *, void *);
void psc_ctlrep_param(int, struct psc_ctlmsghdr *, void *);

void psc_ctlparam_log_level(int, struct psc_ctlmsghdr *,
	struct psc_ctlmsg_param *, char **, int);

void psc_ctlparam_register(char *, void (*)(int, struct psc_ctlmsghdr *,
	struct psc_ctlmsg_param *, char **, int));

void psc_ctlthr_main(const char *, const struct psc_ctlop *, int);
void psc_ctl_applythrop(int, struct psc_ctlmsghdr *, void *, const char *,
	void (*)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *));

extern struct psc_thread pscControlThread;
