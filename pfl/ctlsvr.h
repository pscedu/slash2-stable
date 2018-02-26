/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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
 * Definitions for the daemon live control interface.
 */

#ifndef _PFL_CTLSVR_H_
#define _PFL_CTLSVR_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <ctype.h>
#include <string.h>

#include "pfl/str.h"
#include "pfl/atomic.h"
#include "pfl/thread.h"

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

/*
 * Default control operations shared by all controllable daemons.
 * Must be kept in sync with the PCMT_* list.
 */
#define PSC_CTLDEFOPS								\
	{ NULL /* ERROR */,		0 },					\
	{ psc_ctlrep_getfault,		sizeof(struct psc_ctlmsg_fault) },	\
	{ NULL /* GETFSRQ */,		0 },					\
	{ psc_ctlrep_gethashtable,	sizeof(struct psc_ctlmsg_hashtable) },	\
	{ NULL /* GETJOURNAL */,	0 },					\
	{ psc_ctlrep_getlistcache,	sizeof(struct psc_ctlmsg_listcache) },	\
	{ NULL /* GETLNETIF */,		0 },					\
	{ psc_ctlrep_getmeter,		sizeof(struct psc_ctlmsg_meter) },	\
	{ psc_ctlrep_getmlist,		sizeof(struct psc_ctlmsg_mlist) },	\
	{ psc_ctlrep_getodtable,	sizeof(struct psc_ctlmsg_odtable) },	\
	{ psc_ctlrep_getopstat,		sizeof(struct psc_ctlmsg_opstat) },	\
	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) },	\
	{ psc_ctlrep_getpool,		sizeof(struct psc_ctlmsg_pool) },	\
	{ NULL /* GETRPCRQ */,		0 },					\
	{ NULL /* GETRPCSVC */,		0 },					\
	{ psc_ctlrep_getsubsys,		0 },					\
	{ psc_ctlrep_getthread,		sizeof(struct psc_ctlmsg_thread) },	\
	{ pfl_ctlrep_getworkrq,		sizeof(struct pfl_ctlmsg_workrq) },	\
	{ psc_ctlrep_param,		sizeof(struct psc_ctlmsg_param) }

struct pfl_ctl_data {
	struct sockaddr_un	 pcd_saun;
	struct psc_thread	*pcd_acthr;
	int			 pcd_dead;
	int			 pcd_refcnt;
	int			 pcd_sock;
	struct psc_dynarray	 pcd_clifds;
	psc_spinlock_t		 pcd_lock;
	struct psc_waitq	 pcd_waitq;
	struct pfl_mutex	 pcd_mutex;
};

struct psc_ctlacthr {
	struct pfl_ctl_data	 pcat_ctldata;
};

struct psc_ctlthr {
	int			 pct_nops;
	struct pfl_ctl_data	*pct_ctldata;
	const struct psc_ctlop	*pct_ct;
};

struct psc_ctlop {
	int	(*pc_op)(int, struct psc_ctlmsghdr *, void *);
	size_t	  pc_siz;
};

int	psc_ctlsenderr(int, const struct psc_ctlmsghdr *, struct pfl_mutex *, const char *, ...);
int	psc_ctlmsg_sendv(int, const struct psc_ctlmsghdr *, const void *, struct pfl_mutex *);
int	psc_ctlmsg_send(int, int, int, size_t, const void *, struct pfl_mutex *);

int	psc_ctlrep_getfault(int, struct psc_ctlmsghdr *, void *);
int	pfl_ctlrep_getfsrq(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_gethashtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getjournal(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getlistcache(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getlnetif(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmeter(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getmlist(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getodtable(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getopstat(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getpool(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getrpcrq(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getrpcsvc(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getsubsys(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_getthread(int, struct psc_ctlmsghdr *, void *);
int	pfl_ctlrep_getworkrq(int, struct psc_ctlmsghdr *, void *);
int	psc_ctlrep_param(int, struct psc_ctlmsghdr *, void *);

int	psc_ctlparam_get_vsz(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_get_rss(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);

int	psc_ctlparam_log_console(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_file(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_format(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_level(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_log_points(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_pool(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_rlim(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_rusage(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_run(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_opstats(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_pause(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);
int	psc_ctlparam_faults(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *);

enum pflctl_paramt {
	PFLCTL_PARAMT_NONE,
	PFLCTL_PARAMT_ATOMIC32,
	PFLCTL_PARAMT_INT,
	PFLCTL_PARAMT_STR,
	PFLCTL_PARAMT_UINT64
};

#define PFLCTL_PARAMF_RDWR	(1 << 0)

struct psc_ctlparam_node *
	psc_ctlparam_register(const char *, int (*)(int, struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, char **, int, struct psc_ctlparam_node *));
void	psc_ctlparam_register_simple(const char *, void (*)(char *), int (*)(const char *));
void	psc_ctlparam_register_var(const char *, enum pflctl_paramt, int, void *);

int	psc_ctlmsg_param_send(int, const struct psc_ctlmsghdr *,
		struct psc_ctlmsg_param *, const char *, char **, int, const char *);

void	pfl_ctl_destroy(struct pfl_ctl_data *);
void	psc_ctlthr_main(const char *, const struct psc_ctlop *, int, int, int);
int	psc_ctl_applythrop(int, struct psc_ctlmsghdr *, void *, const char *,
		int (*)(int, struct psc_ctlmsghdr *, void *, struct psc_thread *));

void	psc_ctlthr_mainloop(struct psc_thread *);

#endif /* _PFL_CTLSVR_H_ */
