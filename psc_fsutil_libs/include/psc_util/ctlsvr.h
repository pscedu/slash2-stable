/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2011, Pittsburgh Supercomputing Center (PSC).
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
 * Definitions for the daemon live control interface.
 */

#ifndef _PFL_CTLSVR_H_
#define _PFL_CTLSVR_H_

#include "psc_util/thread.h"

struct psc_ctlmsghdr;
struct psc_ctlmsg_param;
struct psc_ctlmsg_thread;
struct psc_ctlparam_node;

#define PSC_CTL_FOREACH_THREAD(thr, thrname, threads)				\
	psclist_for_each_entry((thr), (threads), pscthr_lentry)			\
		if (strncmp((thr)->pscthr_name, (thrname),			\
		    strlen(thrname)) == 0 ||					\
		    strcmp((thrname), PCTHRNAME_EVERYONE) == 0)

/* XXX use PSCTHR_MKCAST */
#define psc_ctlthr(thr)		((struct psc_ctlthr *)(thr)->pscthr_private)
#define psc_ctlacthr(thr)	((struct psc_ctlacthr *)(thr)->pscthr_private)

/* default control operations shared by all controllable daemons */
#define PSC_CTLDEFOPS								\
	{ NULL,				0 },					\
	{ psc_ctlrep_getfault,		sizeof(struct psc_ctlmsg_fault) },	\
	{ psc_ctlrep_gethashtable,	sizeof(struct psc_ctlmsg_hashtable) },	\
	{ psc_ctlrep_getiostats,	sizeof(struct psc_ctlmsg_iostats) },	\
	{ psc_ctlrep_getjournal,	sizeof(struct psc_ctlmsg_journal) },	\
	{ psc_ctlrep_getlc,		sizeof(struct psc_ctlmsg_lc) },		\
	{ psc_ctlrep_getlni,		sizeof(struct psc_ctlmsg_lni) },	\
	{ psc_ctlrep_getloglevel,	sizeof(struct psc_ctlmsg_loglevel) },	\
	{ psc_ctlrep_getmeter,		sizeof(struct psc_ctlmsg_meter) },	\
	{ psc_ctlrep_getmlist,		sizeof(struct psc_ctlmsg_mlist) },	\
	{ psc_ctlrep_getodtable,	sizeof(struct psc_ctlmsg_odtable) },	\
	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
	{ psc_ctlrep_getpool,		sizeof(struct psc_ctlmsg_pool) },	\
	{ psc_ctlrep_getrpcsvc,		sizeof(struct psc_ctlmsg_rpcsvc) },	\
	{ psc_ctlrep_getsubsys,		0 },					\
	{ psc_ctlrep_getthread,		sizeof(struct psc_ctlmsg_thread) },	\
	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) }

struct psc_ctlacthr {
	int			 pcat_sock;
	struct {
		int nclients;
	}			 pcat_stat;
};

struct psc_ctlthr {
	int			 pct_sockfd;
	const struct psc_ctlop	*pct_ct;
	int			 pct_nops;
	struct {
		int nsent;
		int nrecv;
		int ndrop;
	}			 pct_stat;
};

struct psc_ctlop {
	int	(*pc_op)(int, struct psc_ctlmsghdr *, void *);
	size_t	  pc_siz;
};

int	psc_ctlsenderr(int, const struct psc_ctlmsghdr *, const char *, ...);

int	psc_ctlmsg_sendv(int, const struct psc_ctlmsghdr *, const void *);
int	psc_ctlmsg_send(int, int, int, size_t, const void *);

int	psc_ctlrep_getfault(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_gethashtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getiostats(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getjournal(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getlc(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getlni(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getloglevel(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmeter(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmlist(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getodtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getpool(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getrpcsvc(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getsubsys(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getthread(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_param(int, struct psc_ctlmsghdr *, void *);

void	psc_ctlthr_get(struct psc_thread *, struct psc_ctlmsg_thread *);
void	psc_ctlacthr_get(struct psc_thread *, struct psc_ctlmsg_thread *);

int	psc_ctlparam_log_file(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_format(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_level(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_pool(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_rlim(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_run(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_pause(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_faults(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);

struct psc_ctlparam_node *
	psc_ctlparam_register(const char *, int (*)(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *));
void	psc_ctlparam_register_simple(const char *, void (*)(char *), int (*)(const char *));

int	psc_ctlmsg_param_send(int, const struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, const char *, char **, int, const char *);

__dead void
	psc_ctlthr_main(const char *, const struct psc_ctlop *, int, int);
int	psc_ctl_applythrop(int, struct psc_ctlmsghdr *, void *, const char *,
		int (*)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *));

typedef void (*psc_ctl_thrget_t)(struct psc_thread *, struct psc_ctlmsg_thread *);
extern psc_ctl_thrget_t psc_ctl_thrgets[];
extern int psc_ctl_nthrgets;

#define PFLCTL_SVR_DEFS							\
int psc_ctl_nthrgets = nitems(psc_ctl_thrgets)

#endif /* _PFL_CTLSVR_H_ */
