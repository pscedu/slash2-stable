/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2012, Pittsburgh Supercomputing Center (PSC).
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
 * odtable: on-disk table for persistent storage of otherwise memory
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

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/crc.h"
#include "psc_util/lock.h"
#include "psc_util/log.h"

#define ODT_DEFAULT_ITEM_SIZE	128
#define	ODT_DEFAULT_TABLE_SIZE	(1024 * 128)

#define ODTBL_VERS		UINT64_C(0x0000000000000002)
#define ODTBL_MAGIC		UINT64_C(0x001122335577bbdd)
#define ODTBL_INUSE		UINT64_C(0xf0f0f0f0f0f0f0f0)
#define ODTBL_FREE		UINT64_C(0xffffffffffffffff)
#define ODTBL_BAD		UINT64_C(0x1010101010101010)
#define ODTBL_ERR		((size_t)-1)

enum od_table_errors {
	ODTBL_NO_ERR	= 0,
	ODTBL_MAGIC_ERR	= 1,
	ODTBL_SLOT_ERR	= 2,
	ODTBL_INUSE_ERR	= 3,
	ODTBL_FREE_ERR	= 4,
	ODTBL_CRC_ERR	= 5,
	ODTBL_BADSL_ERR	= 6,
	ODTBL_KEY_ERR	= 7
};

/* Table entries start on a 4k boundary
 */
#define ODTBL_START		0x1000

struct odtable_hdr {
	size_t			 odth_nelems;
	size_t			 odth_elemsz;	/* does not include odtable_entftr */
	size_t			 odth_slotsz;	/* does include odtable_entftr */
	uint64_t		 odth_magic;
	uint32_t		 odth_version;
	uint32_t		 odth_options;	/* see ODTBL_OPT_* below */
	off_t			 odth_start;
} __packed;

/* odtable options */
#define ODTBL_OPT_CRC		(1 << 0)
#define ODTBL_OPT_SYNC		(1 << 1)

/* entry footer */
struct odtable_entftr {
	uint64_t		 odtf_crc;
	uint64_t		 odtf_inuse;
	uint64_t		 odtf_slotno;
	uint64_t		 odtf_magic;
#define odtf_key odtf_crc
};

#define ODT_NAME_MAX		16
#define ODT_DEFAULT_ITEM_SIZE	128

struct odtable {
	struct psc_vbitmap	*odt_bitmap;
	void			*odt_base;
	struct odtable_hdr	*odt_hdr;
	psc_spinlock_t		 odt_lock;
	int			 odt_fd;
	void			*odt_handle;		/* need this for odtable in ZFS */
	char			 odt_name[ODT_NAME_MAX];
	struct psclist_head	 odt_lentry;
};

struct odtable_receipt {
	size_t			 odtr_elem;
	uint64_t		 odtr_key;
};

#define ODTABLE_MAPSZ(odt)	_odtable_getitemoff((odt), (odt)->odt_hdr->odth_nelems, 1)

struct odtable_receipt *
	 odtable_putitem(struct odtable *, void *, size_t);
int	 odtable_create(const char *, size_t, size_t, int);
int	 odtable_freeitem(struct odtable *, struct odtable_receipt *);
void	*odtable_getitem(struct odtable *, const struct odtable_receipt *);
int	 odtable_load(struct odtable **, const char *, const char *, ...);
int	 odtable_release(struct odtable *);
void	 odtable_scan(struct odtable *, void (*)(void *, struct odtable_receipt *));
struct odtable_receipt *
	 odtable_replaceitem(struct odtable *, struct odtable_receipt *, void *, size_t);

extern struct psc_lockedlist psc_odtables;

/**
 * odtable_getitemoff - Get offset of a table entry, which is relative
 * to either the disk file or start of base memory-mapped address.
 */
static __inline size_t
_odtable_getitemoff(const struct odtable *odt, size_t elem, int allow_max)
{
	if (allow_max)
		psc_assert(elem <= odt->odt_hdr->odth_nelems);
	else
		psc_assert(elem < odt->odt_hdr->odth_nelems);
	return (elem * (odt->odt_hdr->odth_elemsz + sizeof(struct odtable_entftr)));
}

#define odtable_getitemoff(odt, elem)	_odtable_getitemoff((odt), (elem), 0)

/**
 * odtable_getitem_foff - Get offset into disk file of table entry.
 */
static __inline size_t
odtable_getitem_foff(const struct odtable *odt, size_t elem)
{
	return (odt->odt_hdr->odth_start + odtable_getitemoff(odt, elem));
}

/**
 * odtable_getitem_addr - Get address of memory-mapped table entry.
 */
#define odtable_getitem_addr(odt, elem)					\
	PSC_AGP((odt)->odt_base, odtable_getitemoff((odt), (elem)))

/**
 * odtable_getfooter - Get address of memory-mapped table entry's footer.
 */
static __inline struct odtable_entftr *
odtable_getfooter(const struct odtable *odt, size_t elem)
{
	return ((void *)((char *)odt->odt_base +
	    odtable_getitemoff(odt, elem) + odt->odt_hdr->odth_elemsz));
}

static __inline int
odtable_createmmap(struct odtable *odt)
{
	odt->odt_base = mmap(NULL, ODTABLE_MAPSZ(odt), PROT_WRITE |
	    PROT_READ, MAP_SHARED, odt->odt_fd,
	    odt->odt_hdr->odth_start);
	if (odt->odt_base == MAP_FAILED)
		return (errno);
	return (0);
}

static __inline int
odtable_freemap(struct odtable *odt)
{
	int rc = 0;

	if (odt->odt_base)
		rc = munmap(odt->odt_base, ODTABLE_MAPSZ(odt));
	odt->odt_base = NULL;
	return (rc);
}

/**
 * odtable_footercheck - Test an item footer for status control.
 * inuse ==  1 --> test the slot assuming it's being used.
 * inuse ==  0 --> test the slot assuming it's NOT being used.
 * inuse == -1 --> test the slot ignoring whether or not it's being used.
 * inuse ==  2 --> test the slot assuming it's being used but ignoring key.
 */
#define odtable_footercheck(odtf, odtr, inuse)				\
	_PFL_RVSTART {							\
		int _rc = 0;						\
									\
		if ((odtf)->odtf_magic != ODTBL_MAGIC)			\
			_rc = ODTBL_MAGIC_ERR;				\
									\
		else if ((odtf)->odtf_slotno != (odtr)->odtr_elem)	\
			_rc = ODTBL_SLOT_ERR;				\
									\
		else if ((odtf)->odtf_inuse == ODTBL_BAD)		\
			_rc = ODTBL_BADSL_ERR;				\
									\
		else if (!(inuse) && (odtf)->odtf_inuse != ODTBL_FREE)	\
			_rc = ODTBL_FREE_ERR;				\
									\
		else if ((inuse) == 1 &&				\
		    ((odtf)->odtf_inuse == ODTBL_INUSE) &&		\
		    ((odtf)->odtf_key != (odtr)->odtr_key))		\
			_rc = ODTBL_KEY_ERR;				\
									\
		else if ((inuse) > 0 &&					\
		    (odtf)->odtf_inuse != ODTBL_INUSE)			\
			_rc = ODTBL_INUSE_ERR;				\
									\
		if (_rc)						\
			psclog_errorx("slot=%zd has error %d",		\
			    (odtr)->odtr_elem, _rc);			\
		_rc;							\
	} _PFL_RVEND

#endif
