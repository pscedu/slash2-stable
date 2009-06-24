#ifndef _PFL_ODTABLE_H_ 
#define _PFL_ODTABLE_H_

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "psc_types.h"
#include "psc_ds/vbitmap.h"
#include "psc_util/crc.h"
#include "psc_util/assert.h"
#include "psc_util/lock.h"

#define ODTBL_VERS  0x0000000000000001ULL
#define ODTBL_MAGIC 0x001122335577bbddULL
#define ODTBL_INUSE 0xf0f0f0f0f0f0f0f0ULL
#define ODTBL_FREE  0xffffffffffffffffULL
#define ODTBL_BAD   0x1010101010101010ULL
#define ODTBL_ERR  ((size_t) -1)

#define ODTBL_OPT_CRC  (1 << 0)
#define ODTBL_OPT_SYNC (1 << 1)

enum od_table_errors {
	ODTBL_MAGIC_ERR = 1, 
	ODTBL_SLOT_ERR  = 2,
	ODTBL_INUSE_ERR = 3,
	ODTBL_FREE_ERR  = 4,
	ODTBL_CRC_ERR   = 5
};

/* Start data on a 4k boundary
 */
#define ODTBL_ALIGN 0x1000

#define ODTABLE_ELEMSZ(odt)					\
	((odt)->odt_hdr->odth_elemsz + sizeof(struct odtable_hdr))

#define ODTABLE_MAPSZ(odt)				\
	(ODTABLE_ELEMSZ(odt) * (odt)->odt_hdr->odth_nelems)

struct odtable_hdr {
	size_t          odth_nelems;
	size_t          odth_elemsz;	
	uint64_t        odth_magic;
	uint32_t        odth_version;
	uint32_t        odth_options;
	off_t           odth_start;
};

struct odtable_entftr {
	psc_crc_t       odtf_crc;
	uint64_t        odtf_inuse;
	uint64_t        odtf_slotno;
	uint64_t        odtf_magic;
};


struct odtable {
	struct vbitmap     *odt_bitmap;
	int                 odt_fd;
	void               *odt_base;
	struct odtable_hdr *odt_hdr;
	psc_spinlock_t      odt_lock;
};


extern int
odtable_create(const char *, size_t, size_t);

extern int
odtable_load(const char *, struct odtable **);

extern void
odtable_scan(struct odtable *, void (*odt_handler)(void *, size_t));

extern int
odtable_release(struct odtable *);

extern size_t
odtable_putitem(struct odtable *, void *);

extern void *
odtable_getitem(struct odtable *, size_t);

extern void
odtable_freeitem(struct odtable *, size_t);

static inline off_t 
odtable_getoffset(const struct odtable *odt, size_t elem)
{
	psc_assert(elem < odt->odt_hdr->odth_nelems);

	return ((off_t)((elem * (odt->odt_hdr->odth_elemsz + sizeof(struct odtable_hdr)))
			+ odt->odt_hdr->odth_start));
}

static inline void * 
odtable_getitem_addr(const struct odtable *odt, size_t elem)
{
	psc_assert(elem < odt->odt_hdr->odth_nelems);

	return ((void *)((elem * (odt->odt_hdr->odth_elemsz + 
			  sizeof(struct odtable_hdr))) + odt->odt_base));
}

static inline struct odtable_entftr *
odtable_getfooter(const struct odtable *odt, size_t elem)
{
	psc_assert(elem < odt->odt_hdr->odth_nelems);

	return ((struct odtable_entftr *)
		((void *)(odt->odt_base) + (elem * 
					    (odt->odt_hdr->odth_elemsz + 
					     sizeof(struct odtable_hdr)))
		 + odt->odt_hdr->odth_elemsz));
}

static inline int
odtable_createmmap(struct odtable *odt)
{
	odt->odt_base = mmap(0, ODTABLE_MAPSZ(odt), (PROT_WRITE | PROT_READ), 
			     MAP_SHARED, odt->odt_fd, 
			     odt->odt_hdr->odth_start);

	if (odt->odt_base == (void *)MAP_FAILED)
		return (-errno);

	return (0);
}

static inline int
odtable_freemap(struct odtable *odt)
{
	return (munmap(odt->odt_base, ODTABLE_MAPSZ(odt)));
}


#define odtable_footercheck(odtf, elem, inuse, rc)      \
        do {                                            \
                rc = 0;                                 \
                if ((odtf)->odtf_magic != ODTBL_MAGIC)  \
                        rc = ODTBL_MAGIC_ERR;           \
                                                        \
                else if ((odtf)->odtf_slotno != elem)   \
                        rc = ODTBL_SLOT_ERR;                    \
                                                                \
                else if ((odtf)->odtf_inuse == ODTBL_BAD)       \
                        rc = ODTBL_BAD;                                 \
									\
                else if (inuse && (inuse > 0) &&			\
			 (odtf)->odtf_inuse != ODTBL_INUSE)		\
                        rc = ODTBL_INUSE_ERR;                           \
                                                                        \
                else if (!inuse && (inuse > 0) &&			\
			 (odtf)->odtf_inuse != ODTBL_FREE)		\
                        rc = ODTBL_FREE_ERR;                            \
									\
                if (rc)                                                 \
                        psc_errorx("slot=%"_P_U64"d has error %"_P_U64"x", \
				   elem, rc);				\
        } while (0)
 
#endif
