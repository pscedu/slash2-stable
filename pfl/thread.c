/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
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
 * Threading library, integrated with logging subsystem along with some
 * other useful utilities.
 */

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_NUMA
#include <cpuset.h>
#endif

#include "pfl/cdefs.h"
#include "pfl/str.h"
#include "pfl/subsys.h"
#include "psc_ds/dynarray.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"
#include "psc_util/lock.h"
#include "psc_util/mem.h"
#include "psc_util/thread.h"

struct psc_dynarray		 pfl_thread_classes = DYNARRAY_INIT;
__static pthread_key_t		 pfl_tlskey;
__static pthread_key_t		 psc_thrkey;
__static struct psc_vbitmap	 psc_uniqthridmap = VBITMAP_INIT_AUTO;
struct psc_lockedlist		 psc_threads =
    PLL_INIT(&psc_threads, struct psc_thread, pscthr_lentry);

/**
 * pscthr_destroy - Thread destructor.
 * @arg: thread data.
 */
__static void
_pscthr_destroy(void *arg)
{
	struct psc_thread *thr = arg;

	psclog_info("thread dying");

	PLL_LOCK(&psc_threads);
	(void)reqlock(&thr->pscthr_lock);
	pll_remove(&psc_threads, thr);
	if (thr->pscthr_uniqid) {
		psc_vbitmap_unset(&psc_uniqthridmap,
		    thr->pscthr_uniqid - 1);
		if (thr->pscthr_uniqid - 1 <
		    psc_vbitmap_getnextpos(&psc_uniqthridmap))
			psc_vbitmap_setnextpos(&psc_uniqthridmap,
			    thr->pscthr_uniqid - 1);
	}
	PLL_ULOCK(&psc_threads);

	if (thr->pscthr_dtor) {
		thr->pscthr_dtor(thr->pscthr_private);
		psc_free(thr->pscthr_private, PAF_NOLOG);
	}
	psc_waitq_destroy(&thr->pscthr_waitq);
	psc_free(thr->pscthr_loglevels, PAF_NOLOG);
	psc_free(thr, PAF_NOLOG);
}

void
_pfl_tls_release(void *arg)
{
	void **tbl = arg;
	int i;

	for (i = 0; i < PFL_TLSIDX_MAX; i++)
		psc_free(tbl[i], PAF_NOGUARD | PAF_NOLOG);
	psc_free(tbl, PAF_NOGUARD | PAF_NOLOG);
}

void *
pfl_tls_get(int idx, size_t len)
{
	void **tbl;
	int rc;

	tbl = pthread_getspecific(pfl_tlskey);
	if (tbl == NULL) {
		tbl = psc_calloc(sizeof(*tbl), PFL_TLSIDX_MAX,
		    PAF_NOLOG | PAF_NOGUARD);
		rc = pthread_setspecific(pfl_tlskey, tbl);
		if (rc)
			errx(1, "pthread_setspecific: %s", strerror(rc));
	}
	if (tbl[idx] == NULL)
		tbl[idx] = psc_alloc(len, PAF_NOLOG | PAF_NOGUARD);
	return (tbl[idx]);
}

/**
 * pscthr_sigusr1 - Catch SIGUSR1: pause the thread.
 * @sig: signal number.
 */
void
_pscthr_sigusr1(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags |= PTF_PAUSED;
	while (thr->pscthr_flags & PTF_PAUSED) {
		psc_waitq_wait(&thr->pscthr_waitq,
		    &thr->pscthr_lock);
		spinlock(&thr->pscthr_lock);
	}
	ureqlock(&thr->pscthr_lock, locked);
}

/**
 * pscthr_sigusr2 - Catch SIGUSR2: unpause the thread.
 * @sig: signal number.
 */
void
_pscthr_sigusr2(__unusedx int sig)
{
	struct psc_thread *thr;
	int locked;

	thr = pscthr_get();
	while (!tryreqlock(&thr->pscthr_lock, &locked))
		sched_yield();
	thr->pscthr_flags &= ~PTF_PAUSED;
	psc_waitq_wakeall(&thr->pscthr_waitq);
	ureqlock(&thr->pscthr_lock, locked);
}

/**
 * pscthrs_init - Initialize threading subsystem.
 */
void
pscthrs_init(void)
{
	int rc;

	rc = pthread_key_create(&psc_thrkey, _pscthr_destroy);
	if (rc)
		errx(1, "pthread_key_create: %s", strerror(rc));

	rc = pthread_key_create(&pfl_tlskey, _pfl_tls_release);
	if (rc)
		errx(1, "pthread_key_create: %s", strerror(rc));
}

/**
 * pscthr_get_canfail - Retrieve thread info from thread-local storage unless
 *	uninitialized.
 */
struct psc_thread *
pscthr_get_canfail(void)
{
	return (pthread_getspecific(psc_thrkey));
}

/**
 * pscthr_get - Retrieve thread info from thread-local storage.
 */
struct psc_thread *
pscthr_get(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	psc_assert(thr);
	return (thr);
}

/**
 * _pscthr_finish_init: Final initilization code common among all
 *	instantiation methods.
 * @thr: thread to finish initializing.
 */
void
_pscthr_finish_init(struct psc_thread *thr)
{
	struct sigaction sa;
	int n, rc;

	if (thr->pscthr_privsiz)
		thr->pscthr_private = psc_alloc(thr->pscthr_privsiz,
		    PAF_NOLOG);

	thr->pscthr_loglevels = psc_alloc(psc_nsubsys *
	    sizeof(*thr->pscthr_loglevels), PAF_NOLOG);
	for (n = 0; n < psc_nsubsys; n++)
		thr->pscthr_loglevels[n] = psc_log_getlevel_ss(n);
	thr->pscthr_pthread = pthread_self();
	thr->pscthr_thrid = pfl_getsysthrid();
	rc = pthread_setspecific(psc_thrkey, thr);
	if (rc)
		psc_fatalx("pthread_setspecific: %s", strerror(rc));

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_sigusr1;
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		psc_fatal("sigaction");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_sigusr2;
	if (sigaction(SIGUSR2, &sa, NULL) == -1)
		psc_fatal("sigaction");

	pll_addtail(&psc_threads, thr);

	/*
	 * Do this allocation now instead during fatal() if malloc is
	 * corrupted.
	 */
	psclog_getdata();
	psclog_info("%s <pthr %"PSCPRI_PTHRT" thrid %d> alive",
	    thr->pscthr_name, thr->pscthr_pthread,
	    thr->pscthr_thrid);
}

/**
 * pscthr_bind_memnode: Bind a thread to a specific NUMA memnode.
 * @thr: thread structure.
 */
void
_pscthr_bind_memnode(struct psc_thread *thr)
{
#ifdef HAVE_NUMA
	struct bitmask *bm;

	if (thr->pscthr_memnid != -1) {
		bm = numa_allocate_nodemask();
		numa_bitmask_clearall(bm);
		numa_bitmask_setbit(bm,
		    cpuset_p_rel_to_sys_mem(pfl_getsysthrid(),
		    thr->pscthr_memnid));
		if (numa_run_on_node_mask(bm) == -1)
			psc_fatal("numa");
		numa_set_membind(bm);
		numa_bitmask_free(bm);
	}
#else
	(void)thr; /* avoid unused warnings */
#endif
}

/**
 * pscthr_begin: Thread start routine.  This routine invokes the
 *	thread's real start routine once it is safe after the thread who
 *	created us has finished our (external) initialization.
 * @arg: thread structure.
 */
__static void *
_pscthr_begin(void *arg)
{
	struct psc_thread *thr, *oldthr = arg;

	_pscthr_bind_memnode(oldthr);

	/* Setup a localized copy of the thread. */
	thr = psc_alloc(sizeof(*thr), PAF_NOLOG);
	INIT_PSC_LISTENTRY(&thr->pscthr_lentry);
	psc_waitq_init(&thr->pscthr_waitq);
	INIT_SPINLOCK(&thr->pscthr_lock);
	spinlock(&thr->pscthr_lock);

	/* Copy values from original. */
	spinlock(&oldthr->pscthr_lock);
	thr->pscthr_type = oldthr->pscthr_type;
	thr->pscthr_startf = oldthr->pscthr_startf;
	thr->pscthr_privsiz = oldthr->pscthr_privsiz;
	thr->pscthr_flags = oldthr->pscthr_flags;
	thr->pscthr_dtor = oldthr->pscthr_dtor;
	strlcpy(thr->pscthr_name, oldthr->pscthr_name,
	    sizeof(thr->pscthr_name));

	_pscthr_finish_init(thr);

	/* Inform the spawner where our memory is. */
	oldthr->pscthr_private = thr;
	psc_waitq_wakeall(&oldthr->pscthr_waitq);

	/* Wait for the spawner to finish us. */
	do {
		psc_waitq_wait(&thr->pscthr_waitq, &thr->pscthr_lock);
		spinlock(&thr->pscthr_lock);
	} while ((thr->pscthr_flags & PTF_READY) == 0);
	freelock(&thr->pscthr_lock);
	thr->pscthr_startf(thr);
	return (thr);
}

/**
 * pscthr_init - Initialize a thread.
 * @type: application-specific thread type.
 * @flags: operational flags.
 * @startf: thread execution routine.  By specifying a NULL routine,
 *	no pthread will be spawned (assuming that an actual pthread
 *	already exists or will be taken care of).
 * @dtor: optional destructor to run when/if thread exits.
 * @privsiz: size of thread-type-specific data.
 * @memnid: memory node ID to allocate memory for this thread.
 * @namefmt: application-specific printf(3) name for thread.
 */
struct psc_thread *
_pscthr_init(int type, int flags, void (*startf)(struct psc_thread *),
    void (*dtor)(void *), size_t privsiz, int memnid,
    const char *namefmt, ...)
{
	struct psc_thread mythr, *thr;
	va_list ap;
	int rc;

	if (flags & PTF_PAUSED)
		psc_fatalx("PTF_PAUSED specified");

	/*
	 * If there is a start routine, we are already in the pthread, *
	 * so the memory should be local.   Otherwise, we'd like to
	 * allocate it within the thread context for local storage.
	 *
	 * Either way, the storage will be released via psc_free() upon
	 * thread exit.
	 */
	thr = startf ? &mythr : psc_alloc(sizeof(*thr), PAF_NOLOG);
	memset(thr, 0, sizeof(*thr));
	INIT_PSC_LISTENTRY(&thr->pscthr_lentry);
	psc_waitq_init(&thr->pscthr_waitq);
	INIT_SPINLOCK_NOLOG(&thr->pscthr_lock);
	thr->pscthr_type = type;
	thr->pscthr_startf = startf;
	thr->pscthr_privsiz = privsiz;
	thr->pscthr_flags = flags | PTF_RUN;
	thr->pscthr_dtor = dtor;
	thr->pscthr_memnid = memnid;

	va_start(ap, namefmt);
	rc = vsnprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
	    namefmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf: %s", namefmt);
	if (rc >= (int)sizeof(thr->pscthr_name))
		psc_fatalx("thread name too long: %s", namefmt);

	/* Pin thread until initialization is complete. */
	spinlock(&thr->pscthr_lock);
	if (startf) {
		/*
		 * Thread will finish initializing in its own context
		 * and set pscthr_private to the location of the new
		 * localized memory.
		 */
		if ((rc = pthread_create(&thr->pscthr_pthread,
		    NULL, _pscthr_begin, thr)) != 0)
			psc_fatalx("pthread_create: %s", strerror(rc));
		psc_waitq_wait(&thr->pscthr_waitq, &thr->pscthr_lock);
		thr = thr->pscthr_private;
		if (thr->pscthr_privsiz == 0)
			pscthr_setready(thr);
	} else {
		/* Initializing our own thread context. */
		_pscthr_bind_memnode(thr);
		_pscthr_finish_init(thr);
	}
	return (thr);
}

/**
 * pscthr_setready - Set thread state to READY.
 * @thr: thread ready to start.
 */
void
pscthr_setready(struct psc_thread *thr)
{
	(void)reqlock(&thr->pscthr_lock);
	thr->pscthr_flags |= PTF_READY;
	psc_waitq_wakeall(&thr->pscthr_waitq);
	freelock(&thr->pscthr_lock);
}

/**
 * psc_log_getlevel - Get thread logging level for the specified subsystem.
 * @subsys: subsystem ID.
 */
int
psc_log_getlevel(int subsys)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (psc_log_getlevel_ss(subsys));
	if (subsys >= psc_nsubsys)
		psc_fatalx("subsystem out of bounds (%d)", subsys);
	return (thr->pscthr_loglevels[subsys]);
}

/**
 * pscthr_setloglevel - Set thread logging level for the specified subsystem.
 * @ssid: subsystem ID to change logging level for.
 * @newlevel: new logging level value to take on.
 */
void
pscthr_setloglevel(int ssid, int newlevel)
{
	struct psc_thread *thr;
	int i;

	thr = pscthr_get();
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		psc_fatalx("log level out of bounds (%d)", newlevel);

	if (ssid == PSS_ALL)
		for (i = 0; i < psc_nsubsys; i++)
			thr->pscthr_loglevels[i] = newlevel;
	else if (ssid >= psc_nsubsys || ssid < 0)
		psc_fatalx("subsystem out of bounds (%d)", ssid);
	else
		thr->pscthr_loglevels[ssid] = newlevel;
}

/**
 * pscthr_getname - Get current thread name.
 */
const char *
pscthr_getname(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (NULL);
	return (thr->pscthr_name);
}

/**
 * pscthr_setpause - Set thread pause state.
 * @thr: the thread.
 * @pauseval: whether to pause or unpause the thread.
 */
void
pscthr_setpause(struct psc_thread *thr, int pauseval)
{
	spinlock(&thr->pscthr_lock);
	if (pauseval ^ (thr->pscthr_flags & PTF_PAUSED))
		pthread_kill(thr->pscthr_pthread,
		    pauseval ? SIGUSR1 : SIGUSR2);
	freelock(&thr->pscthr_lock);
}

/**
 * psc_get_hostname - Override hostname retrieval to access thread-local
 *	storage for hostname.  Local memory improves the speediness of
 *	logging.
 */
const char *
psc_get_hostname(void)
{
	return (psclog_getdata()->pld_hostname);
}

/**
 * pscthr_setrun - Set the PTF_RUN flag of a thread, enabling it to
 *	begin its life.
 * @thr: thread to modify.
 * @run: boolean whether or not the thread should run.
 */
void
pscthr_setrun(struct psc_thread *thr, int run)
{
	int locked;

	locked = reqlock(&thr->pscthr_lock);
	if (run) {
		thr->pscthr_flags |= PTF_RUN;
		psc_waitq_wakeall(&thr->pscthr_waitq);
	} else
		thr->pscthr_flags &= ~PTF_RUN;
	ureqlock(&thr->pscthr_lock, locked);
}

void
pscthr_setdead(struct psc_thread *thr, int dead)
{
	int locked;

	locked = reqlock(&thr->pscthr_lock);
	if (dead)
		thr->pscthr_flags |= PTF_DEAD;
	else
		thr->pscthr_flags &= ~PTF_DEAD;
	ureqlock(&thr->pscthr_lock, locked);
}

/**
 * pscthr_run - Control point of thread main loop.
 */
int
pscthr_run(void)
{
	struct psc_thread *thr;
	int yield = 1, live = 1;

	live = 1;
	yield = 1;
	thr = pscthr_get();
	do {
		spinlock(&thr->pscthr_lock);
		if (thr->pscthr_flags & PTF_DEAD) {
			live = 0;
			break;
		}
		if ((thr->pscthr_flags & PTF_RUN) == 0) {
			psc_waitq_wait(&thr->pscthr_waitq,
			    &thr->pscthr_lock);
			yield = 0;
			continue;
		}
		freelock(&thr->pscthr_lock);
	} while (0);
	if (yield)
		sched_yield();
	return (live);
}

int
pscthr_getuniqid(void)
{
	struct psc_thread *thr;
	size_t pos;

	thr = pscthr_get();
	if (thr->pscthr_uniqid == 0) {
		PLL_LOCK(&psc_threads);
		if (psc_vbitmap_next(&psc_uniqthridmap, &pos) == -1)
			psc_fatal("psc_vbitmap_next");
		PLL_ULOCK(&psc_threads);
		thr->pscthr_uniqid = pos + 1;
	}
	return (thr->pscthr_uniqid);
}

struct psc_thread *
pscthr_getbyname(const char *name)
{
	struct psc_thread *thr;

	PLL_LOCK(&psc_threads);
	PLL_FOREACH(thr, &psc_threads)
		if (strcmp(thr->pscthr_name, name) == 0)
			break;
	PLL_ULOCK(&psc_threads);
	return (thr);
}

struct psc_thread *
pscthr_getbyid(pthread_t id)
{
	struct psc_thread *thr;

	PLL_LOCK(&psc_threads);
	PLL_FOREACH(thr, &psc_threads)
		if (thr->pscthr_pthread == id ||
		    (unsigned long)thr->pscthr_thrid == (unsigned long)id)
			break;
	PLL_ULOCK(&psc_threads);
	return (thr);
}

void
psc_enter_debugger(__unusedx const char *str)
{
	pthread_kill(pthread_self(), SIGINT);
}
