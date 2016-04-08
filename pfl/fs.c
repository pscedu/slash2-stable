/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
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
 * This file contains routines for the stackable file system modules
 * capability of PFL.  Each entry in the pscfs_modules list contains a
 * table of routines implementing the standard file system operations
 * (read, write, open, unlink, etc.).
 */

#include <unistd.h>

#include "pfl/alloc.h"
#include "pfl/fs.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/opstats.h"
#include "pfl/str.h"
#include "pfl/thread.h"

struct psc_spinlock	pflfs_modules_lock = SPINLOCK_INIT;
int			pflfs_modules_modifying;
int			pflfs_modules_pins;

/*
 * Obtain a read lock on the PFL file system modules list.
 */
void
pflfs_modules_rdpin(void)
{
	spinlock(&pflfs_modules_lock);
	while (pflfs_modules_modifying) {
		freelock(&pflfs_modules_lock);
		usleep(1);
		spinlock(&pflfs_modules_lock);
	}
	pflfs_modules_pins++;
	freelock(&pflfs_modules_lock);
}

/*
 * Release a read lock on the PFL file system modules list.
 */
void
pflfs_modules_rdunpin(void)
{
	spinlock(&pflfs_modules_lock);
	psc_assert(pflfs_modules_pins > 0);
	pflfs_modules_pins--;
	freelock(&pflfs_modules_lock);
}

/*
 * Obtain a write lock on the PFL file system modules list.
 */
void
pflfs_modules_wrpin(void)
{
	spinlock(&pflfs_modules_lock);
	while (pflfs_modules_modifying) {
		freelock(&pflfs_modules_lock);
		usleep(1);
		spinlock(&pflfs_modules_lock);
	}
	pflfs_modules_modifying = 1;
	while (pflfs_modules_pins) {
		freelock(&pflfs_modules_lock);
		usleep(1);
		spinlock(&pflfs_modules_lock);
	}
	freelock(&pflfs_modules_lock);
}

/*
 * Release a write lock on the PFL file system modules list.
 */
void
pflfs_modules_wrunpin(void)
{
	spinlock(&pflfs_modules_lock);
	pflfs_modules_modifying = 0;
	freelock(&pflfs_modules_lock);
}

/*
 * Initialize a module for the file system processing stack.
 */
void
pflfs_module_init(struct pscfs *m, const char *opts)
{
	char *opt;

	if (opts && opts[0]) {
		opt = pfl_strdup(opts);
		do {
			psc_dynarray_add(&m->pf_opts, opt);
			opt = strchr(opt, ',');
			if (opt)
				*opt++ = '\0';
		} while (opt);
	}
}

/*
 * Run module-specific thread-local storage constructors on each fsthr.
 */
void
_pflfs_module_init_threads(struct pscfs *m)
{
	struct psc_thread *thr;
	struct pfl_fsthr *pft;

	PLL_LOCK(&psc_threads);
	PLL_FOREACH(thr, &psc_threads)
		if (thr->pscthr_type == PFL_THRT_FS) {
			pft = thr->pscthr_private;
			pft->pft_private = m->pf_thr_init(thr);
		}
	PLL_ULOCK(&psc_threads);
}

/*
 * Run module-specific thread-local storage destructors on each fsthr.
 */
void
_pflfs_module_destroy_threads(struct pscfs *m)
{
	struct psc_thread *thr;
	struct pfl_fsthr *pft;

	PLL_LOCK(&psc_threads);
	PLL_FOREACH(thr, &psc_threads)
		if (thr->pscthr_type == PFL_THRT_FS) {
			pft = thr->pscthr_private;
			m->pf_thr_destroy(pft->pft_private);
			pft->pft_private = NULL;
		}
	PLL_ULOCK(&psc_threads);
}

/*
 * Initialize and push a new module into the file system processing
 * stack.
 */
void
pflfs_module_add(int pos, struct pscfs *m)
{
	m->pf_opst_read_err = pfl_opstat_initf(OPSTF_BASE10,
	    "fs.%s.read.err", m->pf_name);
	m->pf_opst_write_err = pfl_opstat_initf(OPSTF_BASE10,
	    "fs.%s.write.err", m->pf_name);
	m->pf_opst_read_reply =
	    pfl_opstat_init("fs.%s.read.reply", m->pf_name);
	m->pf_opst_write_reply =
	    pfl_opstat_init("fs.%s.write.reply", m->pf_name);

	if (pos == PFLFS_MOD_POS_LAST)
		pos = psc_dynarray_len(&pscfs_modules);
	psc_dynarray_splice(&pscfs_modules, pos, 0, &m, 1);

	if (m->pf_thr_init)
		_pflfs_module_init_threads(m);
}

/*
 * Destroy a file system processing stack module.
 */
void
pflfs_module_destroy(struct pscfs *m)
{
	if (m->pf_handle_destroy)
		m->pf_handle_destroy(NULL);

	if (m->pf_thr_destroy)
		_pflfs_module_destroy_threads(m);

	/*
	 * Since these are registered in pflfs_module_add(), they may
	 * not be available if early module initializtion failed.
	 */
	if (m->pf_opst_read_err) {
		pfl_opstat_destroy(m->pf_opst_read_err);
		pfl_opstat_destroy(m->pf_opst_write_err);
		pfl_opstat_destroy(m->pf_opst_read_reply);
		pfl_opstat_destroy(m->pf_opst_write_reply);
	}

	if (psc_dynarray_len(&m->pf_opts)) {
		char *opts;

		opts = psc_dynarray_getpos(&m->pf_opts, 0);
		PSCFREE(opts);
	}
	psc_dynarray_free(&m->pf_opts);
}

/*
 * Remove a module from the file system processing stack.
 */
struct pscfs *
pflfs_module_remove(int pos)
{
	struct pscfs *m;

	m = psc_dynarray_getpos(&pscfs_modules, pos);
	psc_dynarray_splice(&pscfs_modules, pos, 1, NULL, 0);
	return (m);
}

void *
pfl_fsthr_getpri(struct psc_thread *thr)
{
	struct pfl_fsthr *pft;

	//psc_assert(thr->pscthr_type == PFL_THRT_FSTHR);
	pft = thr->pscthr_private;
	return (pft->pft_private);
}

void
pfl_fsthr_setpri(struct psc_thread *thr, void *data)
{
	struct pfl_fsthr *pft;

	//psc_assert(thr->pscthr_type == PFL_THRT_FSTHR);
	pft = thr->pscthr_private;
	pft->pft_private = data;
}
