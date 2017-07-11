/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
 * Copyright (c) 2007-2015, Pittsburgh Supercomputing Center (PSC).
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
 * odt: on-disk table for persistent storage of otherwise memory
 * resident data structures.
 */

#ifndef _PFL_ODTABLE_H_
#define _PFL_ODTABLE_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#include "pfl/crc.h"
#include "pfl/err.h"
#include "pfl/opstats.h"
#include "pfl/list.h"
#include "pfl/lock.h"
#include "pfl/lockedlist.h"
#include "pfl/log.h"
#include "pfl/vbitmap.h"

struct pfl_odt;
struct pfl_odt_receipt;

#define ODT_ITEM_SIZE		(128 - sizeof(struct pfl_odt_slotftr))
#define ODT_ITEM_START		0x1000
#define ODT_ITEM_COUNT		(1024 * 128)

/* 
 *  Layout of the on-disk table.  Each slot is 128 bytes and contains one item:
 *
 *    +-----------------+-----+-----+-----+-----+              +-----+
 *    |  header         |     |     |     |     |  .....       |     |
 *    +-----------------+-----+-----+-----+-----+              +-----+
 *    0               0x1000
 *
 *  The initial file size is 1024 * 128 * 128 + 0x1000 = 16781312 bytes.
 */
struct pfl_odt_hdr {
	uint32_t		 odth_nitems;
	uint32_t		 odth_itemsz;	/* does not include pfl_odt_slotftr */
	uint32_t		 odth_slotsz;	/* does include pfl_odt_slotftr */
	uint32_t		 odth_options;	/* see ODTBL_OPT_* below */
	off_t			 odth_start;	/* offset of the first item */
	uint64_t		 odth_crc;	/* CRC of the header */
} __packed;

/* odth_options */
#define ODTBL_OPT_CRC		(1 << 0)
#define ODTBL_OPT_SYNC		(1 << 1)

/* slot footer */
struct pfl_odt_slotftr {
	uint32_t		 odtf_flags;
	uint32_t		 odtf_slotno;
	uint64_t		 odtf_crc;	/* CRC of the footer itself */
};

/* odtf_flags values */
#define ODT_FTRF_INUSE		(1 << 0)

/* pfl_odtops_mmap and slm_odtops */
struct pfl_odt_ops {
	void	(*odtop_new)(struct pfl_odt *, const char *, int);
	void	(*odtop_open)(struct pfl_odt *, const char *, int);
	void	(*odtop_close)(struct pfl_odt *);
	void	(*odtop_read)(struct pfl_odt *,
		    const struct pfl_odt_receipt *, void *,
		    struct pfl_odt_slotftr *);
	void	(*odtop_write)(struct pfl_odt *, const void *,
		    struct pfl_odt_slotftr *, size_t);
	/*
	 * Allow the use of mmap() interface to work on the table.
	 * Currently only used by the odtable (not by slashd).
	 */
	void	(*odtop_mapslot)(struct pfl_odt *, size_t, void **,
		    struct pfl_odt_slotftr **);
	void	(*odtop_resize)(struct pfl_odt *);
	void	(*odtop_sync)(struct pfl_odt *, size_t);
};

struct pfl_odt_stats {
	uint32_t		odst_read_error;
	uint32_t		odst_write_error;
	uint32_t		odst_extend;
	uint32_t		odst_full;
	uint32_t		odst_replace;
	uint32_t		odst_free;
	uint32_t		odst_read;
	uint32_t		odst_write;
};

#define ODT_NAME_MAX		16

struct pfl_odt {
	struct psc_vbitmap	*odt_bitmap;
	struct pfl_odt_hdr	*odt_hdr;
	psc_spinlock_t		 odt_lock;
	struct pfl_odt_ops	 odt_ops;
	union {
		int	 	 odtu_fd;
		void		*odtu_mfh;		/* need this for odtable in ZFS */
	} u;
#define odt_fd		u.odtu_fd
#define odt_mfh		u.odtu_mfh
	char			 odt_name[ODT_NAME_MAX];
	struct psclist_head	 odt_lentry;
	struct pfl_iostats_rw	 odt_iostats;
	struct pfl_odt_stats	 odt_stats;
};

#define ODT_STAT_INCR(t, stat)						\
	do {								\
		int _waslocked;						\
									\
		_waslocked = reqlock(&(t)->odt_lock);			\
		(t)->odt_stats.odst_ ## stat ++;			\
		ureqlock(&(t)->odt_lock, _waslocked);			\
	} while (0)

#define PFLOG_ODT(lvl, t, fmt, ...)					\
	psclog((lvl), "odt@%p[%s] nitems=%u objsz=%u slotsz=%u "	\
	    "opt=%#x :: " fmt,						\
	    (t), (t)->odt_name, (t)->odt_hdr->odth_nitems,		\
	    (t)->odt_hdr->odth_itemsz, (t)->odt_hdr->odth_slotsz,	\
	    (t)->odt_hdr->odth_options, ## __VA_ARGS__)

struct pfl_odt_receipt {
	uint64_t		 odtr_item;
	uint64_t		 odtr_crc;
};

#define ODTBL_FLG_RDONLY	(1 << 0)

#define ODTBL_SLOT_INV		((size_t)-1)

struct pfl_odt_receipt *
	 pfl_odt_additem(struct pfl_odt *, void *);
size_t	 pfl_odt_allocslot(struct pfl_odt *);
void	 pfl_odt_check(struct pfl_odt *,
	    void (*)(void *, struct pfl_odt_receipt *, void *), void *);
void	 pfl_odt_create(const char *, size_t, size_t, int, size_t,
	    size_t, int);
void	 pfl_odt_mapslot(struct pfl_odt *, size_t, void *,
	    struct pfl_odt_slotftr **);
void	 pfl_odt_freeitem(struct pfl_odt *, struct pfl_odt_receipt *);
void	 pfl_odt_freebuf(struct pfl_odt *, void *,
	    struct pfl_odt_slotftr *);
void	 pfl_odt_getslot(struct pfl_odt *,
	    const struct pfl_odt_receipt *, void *,
	    struct pfl_odt_slotftr **);
void	 pfl_odt_load(struct pfl_odt **, struct pfl_odt_ops *, int,
	    const char *, const char *, ...);
struct pfl_odt_receipt *
	 pfl_odt_putitemf(struct pfl_odt *, size_t, void *, int);
void	 pfl_odt_release(struct pfl_odt *);
void	 pfl_odt_replaceitem(struct pfl_odt *, struct pfl_odt_receipt *,
	    void *);

#define pfl_odt_putitem(t, n, p)	pfl_odt_putitemf((t), (n), (p), 1)
#define pfl_odt_getitem(t, r, p)	pfl_odt_getslot((t), (r), (p), NULL)
#define pfl_odt_mapitem(t, n, p)	pfl_odt_mapslot((t), (n), (p), NULL)

extern struct psc_lockedlist pfl_odtables;

/**
 * odtable_footercheck - Test an in-use item's footer for validity.
 */
#define pfl_odt_footercheck(t, f, r)					\
	_PFL_RVSTART {							\
		int _rc = 0;						\
									\
		if ((f)->odtf_slotno != (r)->odtr_item)			\
			_rc = PFLERR_NOKEY;				\
									\
		else if ((f)->odtf_crc != (r)->odtr_crc)		\
			_rc = PFLERR_BADCRC;				\
									\
		if (_rc)						\
			PFLOG_ODT(PLL_ERROR, t,				\
			    "slot=%zd (%u) has error %d; "		\
			    "ftr_crc %"PRIx64" rcpt_crc %"PRIx64,	\
			    (r)->odtr_item, (f)->odtf_slotno, _rc,	\
			    (f)->odtf_crc, (r)->odtr_crc);		\
		_rc;							\
	} _PFL_RVEND

extern struct pfl_odt_ops pfl_odtops_mmap;

#endif
