/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
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
 * Threading library, integrated with logging subsystem along with some
 * other useful utilities.
 */

#ifdef HAVE_LIBPTHREAD

#include <err.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_NUMA
#include <cpuset.h>
#endif

#include "pfl/alloc.h"
#include "pfl/cdefs.h"
#include "pfl/dynarray.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/mem.h"
#include "pfl/opstats.h"
#include "pfl/str.h"
#include "pfl/subsys.h"
#include "pfl/thread.h"
#include "pfl/vbitmap.h"

struct psc_dynarray		 pfl_thread_classes = DYNARRAY_INIT;
__static pthread_key_t		 psc_thrkey;
__static struct psc_vbitmap	 psc_uniqthridmap = VBITMAP_INIT_AUTO;
struct psc_lockedlist		 psc_threads =
    PLL_INIT_NOLOG(&psc_threads, struct psc_thread, pscthr_lentry);

/*
 * The following does not affect ZFS-fuse threads.
 */
#define	PTHREAD_GUARD_SIZE	 4096
#define	PTHREAD_STACK_SIZE	 8*1024*1024

__static pthread_attr_t		 pthread_attr;
__static psc_spinlock_t	  	 pthread_lock;
__static struct psc_waitq	 pthread_waitq;

/*
 * Thread destructor.
 * @arg: thread data.
 */
__static void
_pscthr_destroy(void *arg)
{
	struct psc_thread *thr = arg;

	psclog_diag("thread dying");

	OPSTAT_INCR("pfl.thread-destroy");
	PLL_LOCK(&psc_threads);
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
	/* crash below @40977 */
	psc_free(thr->pscthr_loglevels, PAF_NOLOG);
	psc_free(thr->pscthr_callerinfo, PAF_NOLOG);
	psc_free(thr, PAF_NOLOG);
}

void
pscthr_destroy(struct psc_thread *arg)
{
	_pscthr_destroy(arg);
}

/*
 * Pause thread execution.
 * @sig: signal number.
 */
void
_pscthr_pause(__unusedx int sig)
{
	struct psc_thread *thr;

	thr = pscthr_get();
	while (!trylock(&pthread_lock))
		pscthr_yield();
	thr->pscthr_flags |= PTF_PAUSED;
	while (thr->pscthr_flags & PTF_PAUSED) {
		psc_waitq_wait(&pthread_waitq,
		    &pthread_lock);
		spinlock(&pthread_lock);
	}
	freelock(&pthread_lock);
}

/*
 * Unpause thread execution.
 * @sig: signal number.
 */
void
_pscthr_unpause(__unusedx int sig)
{
	struct psc_thread *thr;

	thr = pscthr_get();
	while (!trylock(&pthread_lock))
		pscthr_yield();
	thr->pscthr_flags &= ~PTF_PAUSED;
	psc_waitq_wakeall(&pthread_waitq);
	freelock(&pthread_lock);
}

/*
 * Initialize threading subsystem.
 */
void
pscthrs_init(void)
{
	int rc;

	INIT_SPINLOCK_NOLOG(&pthread_lock);
	psc_waitq_init_nolog(&pthread_waitq, "thrs_wait");

	rc = pthread_key_create(&psc_thrkey, _pscthr_destroy);
	if (rc)
		errx(1, "pthread_key_create: %s", strerror(rc));

	pthread_attr_init(&pthread_attr);
	rc = pthread_attr_setstacksize(&pthread_attr, PTHREAD_STACK_SIZE);
	if (rc)
		errx(1, "pthread_attr_setstacksize: %s", strerror(rc));
	rc = pthread_attr_setguardsize(&pthread_attr, PTHREAD_GUARD_SIZE);
	if (rc)
		errx(1, "pthread_attr_setguardsize: %s", strerror(rc));
}

/*
 * Retrieve thread info from thread-local storage unless uninitialized.
 */
struct psc_thread *
pscthr_get_canfail(void)
{
	return (pthread_getspecific(psc_thrkey));
}

/*
 * Retrieve thread info from thread-local storage.
 */
struct psc_thread *
pscthr_get(void)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	return (thr);
}

/*
 * Final initilization code common among all instantiation methods.
 * @thr: thread to finish initializing.
 */
void
_pscthr_finish_init(struct psc_thread_init *thr_init)
{
	struct sigaction sa;
	struct psc_thread *thr;
	int n, rc;

	thr = thr_init->pti_thread;
	thr->pscthr_loglevels = psc_alloc(psc_dynarray_len(
	    &pfl_subsystems) * sizeof(*thr->pscthr_loglevels),
	    PAF_NOLOG);
	for (n = 0; n < psc_dynarray_len(&pfl_subsystems); n++)
		thr->pscthr_loglevels[n] = psc_log_getlevel_ss(n);
	thr->pscthr_pthread = pthread_self();
	thr->pscthr_thrid = pfl_getsysthrid();
	rc = pthread_setspecific(psc_thrkey, thr);
	if (rc)
		psc_fatalx("pthread_setspecific: %s", strerror(rc));

#if 0
	/* this trigger crash in the pscfs_fuse_listener_loop() */
	rc = pthread_detach(thr->pscthr_pthread);
	if (rc)
		psc_fatalx("pthread_detach: %s", strerror(rc));
#endif

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_pause;
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		psc_fatal("sigaction");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _pscthr_unpause;
	if (sigaction(SIGUSR2, &sa, NULL) == -1)
		psc_fatal("sigaction");

	/*
	 * Do this allocation now instead during fatal() if malloc is
	 * corrupted.
	 */
	thr->pscthr_callerinfo = psc_alloc(sizeof(struct pfl_callerinfo), 
	    PAF_NOLOG);

	/* 
	 * See psc_ctlmsg_thread_send() on how to return thread
	 * formation via command like msctl -s threads.
	 */
	pll_addtail(&psc_threads, thr);

	psclog_info("%s <pthr %"PSCPRI_PTHRT" thrid %d> alive",
	    thr->pscthr_name, thr->pscthr_pthread,
	    thr->pscthr_thrid);
}

/*
 * Bind a thread to a specific NUMA memnode.
 * @thr: thread structure.
 */
void
_pscthr_bind_memnode(struct psc_thread_init *thr_init)
{
#ifdef HAVE_NUMA

	struct bitmask *bm;
	if (thr_init->pti_memnid != -1) {
		bm = numa_allocate_nodemask();
		numa_bitmask_clearall(bm);
		numa_bitmask_setbit(bm,
		    cpuset_p_rel_to_sys_mem(pfl_getsysthrid(),
		    thr_init->pti_memnid));
		if (numa_run_on_node_mask(bm) == -1)
			psc_fatal("numa");
		numa_set_membind(bm);
		numa_bitmask_free(bm);
	}
#else
	(void)thr_init; /* avoid unused warnings */
#endif
}

/*
 * Thread start routine.  This routine invokes the thread's real start
 * routine once it is safe after the thread who created us has finished
 * our (external) initialization.
 * @arg: thread structure.
 */
__static void *
_pscthr_begin(void *arg)
{
	struct psc_thread *thr;
	void (*startf)(struct psc_thread *);
	struct psc_thread_init *thr_init = arg;

	thr = thr_init->pti_thread;
	startf = thr_init->pti_startf;

	spinlock(&pthread_lock);
	_pscthr_bind_memnode(thr_init);
	_pscthr_finish_init(thr_init);
	thr->pscthr_flags &= ~PTF_INIT;
	psc_waitq_wakeall(&pthread_waitq);

	/* Wait for the spawner to finish us. */
	do {
		psc_waitq_wait(&pthread_waitq, &pthread_lock);
		spinlock(&pthread_lock);
	} while ((thr->pscthr_flags & PTF_READY) == 0);
	freelock(&pthread_lock);
	startf(thr);
	return (thr);
}

/*
 * Initialize a thread.
 * @type: application-specific thread type.
 * @startf: thread execution routine.  By specifying a NULL routine, no
 * pthread will be spawned (assuming that an actual pthread already
 * exists or will be taken care of).
 * @privsiz: size of thread-type-specific data.
 * @memnid: memory node ID to allocate memory for this thread.
 * @namefmt: application-specific printf(3) name for thread.
 */
struct psc_thread *
_pscthr_init(int type, void (*startf)(struct psc_thread *),
    size_t privsiz, int memnid, const char *namefmt, ...)
{
	struct psc_thread *thr;
	struct psc_thread_init thr_init;
	va_list ap;
	int rc;

	thr = psc_alloc(sizeof(*thr) + privsiz, PAF_NOLOG | PAF_NOZERO);
	memset(thr, 0, sizeof(*thr) + privsiz);
	INIT_PSC_LISTENTRY(&thr->pscthr_lentry);
	thr->pscthr_type = type;
	thr->pscthr_private = (void *)(thr + 1);
	thr->pscthr_flags = PTF_RUN | PTF_INIT;

	thr_init.pti_thread = thr;
	thr_init.pti_startf = startf;
	thr_init.pti_memnid = memnid;

	va_start(ap, namefmt);
	rc = vsnprintf(thr->pscthr_name, sizeof(thr->pscthr_name),
	    namefmt, ap);
	va_end(ap);

	if (rc == -1)
		psc_fatal("vsnprintf: %s", namefmt);
	if (rc >= (int)sizeof(thr->pscthr_name))
		psc_fatalx("thread name too long: %s", namefmt);

	/* Pin thread until initialization is complete. */
	spinlock(&pthread_lock);
	if (startf) {
		rc = pthread_create(&thr->pscthr_pthread, &pthread_attr, 
		    _pscthr_begin, &thr_init);
		if (rc)
			psc_fatalx("pthread_create: %s", strerror(rc));
		while (thr->pscthr_flags & PTF_INIT) {
			psc_waitq_wait(&pthread_waitq, &pthread_lock);
			spinlock(&pthread_lock);
		}
		freelock(&pthread_lock);
		/* Automatically set ready if no private data */
		if (privsiz == 0)
			pscthr_setready(thr);
	} else {
		/* Initializing our own thread context. */
		_pscthr_bind_memnode(&thr_init);
		_pscthr_finish_init(&thr_init);
		freelock(&pthread_lock);
	}
	OPSTAT_INCR("pfl.thread-create");
	return (thr);
}

/*
 * Set thread state to READY.
 * @thr: thread ready to start.
 */
void
pscthr_setready(struct psc_thread *thr)
{
	spinlock(&pthread_lock);
	thr->pscthr_flags |= PTF_READY;
	psc_waitq_wakeall(&pthread_waitq);
	freelock(&pthread_lock);
}

int
psc_log_getlevel(int subsys)
{
	struct psc_thread *thr;

	thr = pscthr_get_canfail();
	if (thr == NULL)
		return (psc_log_getlevel_ss(subsys));
	if (subsys >= psc_dynarray_len(&pfl_subsystems))
		psc_fatalx("subsystem %d out of bounds (%d)", subsys,
		    psc_dynarray_len(&pfl_subsystems));
	return (thr->pscthr_loglevels[subsys]);
}

void
psc_log_setlevel(int ssid, int newlevel)
{
	struct psc_thread *thr;
	int i;

	thr = pscthr_get();
	if (newlevel >= PNLOGLEVELS || newlevel < 0)
		psc_fatalx("log level out of bounds (%d)", newlevel);

	if (ssid == PSS_ALL)
		for (i = 0; i < psc_dynarray_len(&pfl_subsystems); i++)
			thr->pscthr_loglevels[i] = newlevel;
	else if (ssid >= psc_dynarray_len(&pfl_subsystems) || ssid < 0)
		psc_fatalx("subsystem out of bounds (%d)", ssid);
	else
		thr->pscthr_loglevels[ssid] = newlevel;
}

/*
 * Set thread pause state.
 * @thr: the thread.
 * @pauseval: whether to pause or unpause the thread.
 */
void
pscthr_setpause(struct psc_thread *thr, int pauseval)
{
	spinlock(&pthread_lock);
	if (pauseval ^ (thr->pscthr_flags & PTF_PAUSED))
		pthread_kill(thr->pscthr_pthread,
		    pauseval ? SIGUSR1 : SIGUSR2);
	freelock(&pthread_lock);
}

/*
 * Set the PTF_RUN flag of a thread, enabling it to begin its life.
 * @thr: thread to modify.
 * @run: boolean whether or not the thread should run.
 */
void
pscthr_setrun(struct psc_thread *thr, int run)
{
	spinlock(&pthread_lock);
	if (run) {
		thr->pscthr_flags |= PTF_RUN;
		psc_waitq_wakeall(&pthread_waitq);
	} else
		thr->pscthr_flags &= ~PTF_RUN;
	freelock(&pthread_lock);
}

void
pscthr_setdead(struct psc_thread *thr, int dead)
{
	spinlock(&pthread_lock);
	if (dead)
		thr->pscthr_flags |= PTF_DEAD;
	else
		thr->pscthr_flags &= ~PTF_DEAD;
	freelock(&pthread_lock);
}

/*
 * Control point of thread main loop.
 */
int
pscthr_run(struct psc_thread *thr)
{
	if (thr->pscthr_flags & PTF_DEAD)
		return (0);
	if ((thr->pscthr_flags & PTF_RUN) == 0) {
		spinlock(&pthread_lock);
		while ((thr->pscthr_flags & PTF_RUN) == 0) {
			psc_waitq_wait(&pthread_waitq,
			    &pthread_lock);
			spinlock(&pthread_lock);
		}
		freelock(&pthread_lock);
	}
	return (1);
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
pscthr_killall(void)
{
	struct psc_thread *thr;

	PLL_LOCK(&psc_threads);
	PLL_FOREACH(thr, &psc_threads) {
		spinlock(&pthread_lock);
		thr->pscthr_flags |= PTF_DEAD;
		freelock(&pthread_lock);
	}
	PLL_ULOCK(&psc_threads);
}

struct pfl_callerinfo *
pscthr_get_callerinfo(void)
{
	struct psc_thread *thr;
	struct pfl_callerinfo *pci;
	static struct pfl_callerinfo tmp_pci;

	thr = pscthr_get_canfail();
	if (thr)
		pci = thr->pscthr_callerinfo;
	else
		pci = &tmp_pci;

	return (pci);
}

#else

struct struct pfl_callerinfo *
pscthr_get_callerinfo(void)
{
	static struct pfl_callerinfo tmp_pci;
	return (&tmp_pci);
}

struct psc_thread *
pscthr_get_canfail(void)
{
	return (NULL);
}

#endif
