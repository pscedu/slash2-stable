/* $Id$ */
/* %ISC_COPYRIGHT% */

/*
 * This file contains routines for the stackable file system modules capability
 * of PFL.  Each entry in the pscfs_modules list contains a table of routines
 * implementing the standard file system operations (read, write, open, unlink,
 * etc.).
 */

#include <unistd.h>

#include "pfl/fs.h"
#include "pfl/lock.h"
#include "pfl/log.h"
#include "pfl/opstats.h"

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
pflfs_module_init(struct pscfs *m)
{
	m->pf_opst_read_err = pfl_opstat_initf(OPSTF_BASE10,
	    "fs.%s.read.err", m->pf_name);
	m->pf_opst_write_err = pfl_opstat_initf(OPSTF_BASE10,
	    "fs.%s.write.err", m->pf_name);
	m->pf_opst_read_reply =
	    pfl_opstat_init("fs.%s.read.reply", m->pf_name);
	m->pf_opst_write_reply =
	    pfl_opstat_init("fs.%s.write.reply", m->pf_name);
}

/*
 * Initialize and push a new module into the file system processing
 * stack.
 */
void
pflfs_module_add(int pos, struct pscfs *m)
{
	if (pos == PFLFS_MOD_POS_LAST)
		pos = psc_dynarray_len(&pscfs_modules);
	psc_dynarray_splice(&pscfs_modules, pos, 0, &m, 1);
}

/*
 * Destroy a file system processing stack module.
 */
void
pflfs_module_destroy(struct pscfs *m)
{
	if (m->pf_handle_destroy)
		m->pf_handle_destroy(NULL);

	pfl_opstat_destroy(m->pf_opst_read_err);
	pfl_opstat_destroy(m->pf_opst_write_err);
	pfl_opstat_destroy(m->pf_opst_read_reply);
	pfl_opstat_destroy(m->pf_opst_write_reply);
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
