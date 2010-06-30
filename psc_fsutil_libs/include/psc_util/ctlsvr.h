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

#include "psc_util/thread.h"

struct psc_ctlmsghdr;
struct psc_ctlmsg_param;
struct psc_ctlmsg_stats;

#define PSC_CTL_FOREACH_THREAD(thr, thrname, threads)				\
	psclist_for_each_entry((thr), (threads), pscthr_lentry)			\
		if (strncmp((thr)->pscthr_name, (thrname),			\
		    strlen(thrname)) == 0 ||					\
		    strcmp((thrname), PCTHRNAME_EVERYONE) == 0)

/* XXX use PSCTHR_MKCAST */
#define psc_ctlthr(thr)	((struct psc_ctlthr *)(thr)->pscthr_private)

/* default control operations shared by all controllable daemons */
#define PSC_CTLDEFOPS								\
/* 0 */	{ NULL,				0 },					\
/* 1 */	{ psc_ctlrep_getloglevel,	sizeof(struct psc_ctlmsg_loglevel) },	\
/* 2 */	{ psc_ctlrep_getlc,		sizeof(struct psc_ctlmsg_lc) },		\
/* 3 */	{ psc_ctlrep_getstats,		sizeof(struct psc_ctlmsg_stats) },	\
/* 4 */	{ psc_ctlrep_getsubsys,		0 },					\
/* 5 */	{ psc_ctlrep_gethashtable,	sizeof(struct psc_ctlmsg_hashtable) },	\
/* 6 */	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
/* 7 */	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
/* 8 */	{ psc_ctlrep_getiostats,	sizeof(struct psc_ctlmsg_iostats) },	\
/* 9 */	{ psc_ctlrep_getmeter,		sizeof(struct psc_ctlmsg_meter) },	\
/*10 */	{ psc_ctlrep_getpool,		sizeof(struct psc_ctlmsg_pool) },	\
/*11 */	{ psc_ctlrep_getmlist,		sizeof(struct psc_ctlmsg_mlist) },	\
/*12 */	{ psc_ctlrep_getfault,		sizeof(struct psc_ctlmsg_fault) },	\
/*12 */	{ psc_ctlrep_getodtable,	sizeof(struct psc_ctlmsg_odtable) },	\
/*13 */	{ psc_ctlhnd_cmd,		sizeof(struct psc_ctlmsg_cmd) }

struct psc_ctlthr {
	int	  pc_st_nclients;
	int	  pc_st_nsent;
	int	  pc_st_nrecv;
	int	  pc_st_ndrop;
};

struct psc_ctlop {
	int	(*pc_op)(int, struct psc_ctlmsghdr *, void *);
	size_t	  pc_siz;
};

int	psc_ctlsenderr(int, struct psc_ctlmsghdr *, const char *, ...);

int	psc_ctlmsg_sendv(int, const struct psc_ctlmsghdr *, const void *);
int	psc_ctlmsg_send(int, int, int, size_t, const void *);

int	psc_ctlrep_getfault(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_gethashtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getiostats(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getlc(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getloglevel(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmeter(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmlist(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getodtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getpool(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getstats(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getsubsys(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_param(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlhnd_cmd(int, struct psc_ctlmsghdr *, void *);

void	psc_ctlthr_stat(struct psc_thread *, struct psc_ctlmsg_stats *);

int	psc_ctlparam_log_file(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_log_format(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_log_level(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_pool(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_rlim_nofile(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_run(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_pause(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);
int	psc_ctlparam_faults(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int);

void	psc_ctlparam_register(const char *, int (*)(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int));

int	psc_ctlmsg_param_send(int, const struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, const char *, char **, int, const char *);

__dead void
	psc_ctlthr_main(const char *, const struct psc_ctlop *, int);
int	psc_ctl_applythrop(int, struct psc_ctlmsghdr *, void *, const char *,
		int (*)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *));

extern void (*psc_ctl_getstats[])(struct psc_thread *, struct psc_ctlmsg_stats *);
extern int psc_ctl_ngetstats;

extern int (*psc_ctl_cmds[])(int, struct psc_ctlmsghdr *, void *);
extern int psc_ctl_ncmds;
