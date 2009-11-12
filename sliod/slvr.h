/* $Id$ */

#ifndef _SLASH_SLVR_H_
#define _SLASH_SLVR_H_

#include "psc_ds/dynarray.h"
#include "psc_ds/listcache.h"
#include "psc_ds/tree.h"
#include "psc_util/atomic.h"
#include "psc_util/log.h"

#include "slashrpc.h"
#include "bmap.h"
#include "buffer.h"

struct bmap_iod_info;

extern struct psc_listcache lruSlvrs;
extern struct psc_listcache rpcqSlvrs;
extern struct psc_listcache inflSlvrs;

/**
 * slvr_ref - sliver reference used for scheduling dirty slivers to be crc'd and sent to the mds.
 * Note: slivers are locked through their bmap_iod_info lock.
 */
struct slvr_ref {
	uint16_t		 slvr_num;		/* sliver index in the block map */
	uint16_t		 slvr_flags;
	uint16_t		 slvr_pndgwrts;		/* # of writes in progess */
	uint16_t		 slvr_pndgreads;	/* # of reads in progress */
	psc_crc_t		 slvr_crc;		/* used if there's no bmap_wire present, only is valid if !SLVR_CRCDIRTY */
	void			*slvr_pri;		/* private pointer used for backpointer to bmap_iod_info */
	struct sl_buffer	*slvr_slab;
	struct psclist_head	 slvr_lentry;		/* dirty queue */	
	SPLAY_ENTRY(slvr_ref)	 slvr_tentry;		/* bmap tree entry */
};

#define SLVR_2_BLK(s) ((s)->slvr_num * (SLASH_BMAP_SIZE/SLASH_BMAP_BLKSZ))

enum slvr_states {
	SLVR_NEW	= (1 <<  0),	/* newly initialized */
	SLVR_SPLAYTREE	= (1 <<  1),	/* registered in the splay tree */
	SLVR_CRCING	= (1 <<  2),	/* in the process of being crc'd */
	SLVR_FAULTING	= (1 <<  3),	/* one or more blocks are being read from the fs */
	SLVR_INFLIGHT	= (1 <<  4),	/* slvr crc is being sent the mds */ 
	SLVR_GETSLAB	= (1 <<  5),	/* assigning memory buffer to slvr */
	SLVR_PINNED	= (1 <<  6),	/* slab cannot be removed from the cache */
	SLVR_DATARDY	= (1 <<  7),	/* ready for read / write activity */
	SLVR_LRU	= (1 <<  8),	/* cached but not dirty */
	SLVR_CRCDIRTY	= (1 <<  9),	/* crc does not match cached buffer */
	SLVR_RPCPNDG	= (1 << 10),	/* buffer !dirty but crc dirty is set */
	SLVR_FREEING	= (1 << 11),	/* sliver is being reaped */
	SLVR_SLBFREEING	= (1 << 12)	/* sliver is being reaped */
};


#define SLVR_FLAG(field, str) ((field) ? (str) : "")
#define DEBUG_SLVR_FLAGS(s)						\
	SLVR_FLAG(((s)->slvr_flags & SLVR_NEW), "n"),			\
		SLVR_FLAG(((s)->slvr_flags & SLVR_CRCING), "c"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_FAULTING), "f"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_INFLIGHT), "i"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_GETSLAB), "G"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_PINNED), "p"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_CRCDIRTY), "D"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_DATARDY), "d"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_LRU), "l"),		\
		SLVR_FLAG(((s)->slvr_flags & SLVR_RPCPNDG), "r"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_FREEING), "F"),	\
		SLVR_FLAG(((s)->slvr_flags & SLVR_SLBFREEING), "b")

#define SLVR_FLAGS_FMT "%s%s%s%s%s%s%s%s%s%s%s%s"

#define DEBUG_SLVR(level, s, fmt, ...)					\
	psc_logs((level), PSS_GEN,					\
		 " slvr@%p num=%hu pw=%hu pr=%hu pri@%p slab@%p flgs:"	\
		 SLVR_FLAGS_FMT" :: "fmt,				\
		 (s), (s)->slvr_num,					\
		 (s)->slvr_pndgwrts,					\
		 (s)->slvr_pndgreads,					\
		 (s)->slvr_pri, (s)->slvr_slab, DEBUG_SLVR_FLAGS(s),	\
		 ## __VA_ARGS__)

static inline int
slvr_cmp(const void *x, const void *y)
{
        const struct slvr_ref *a = x, *b = y;

        if (a->slvr_num > b->slvr_num)
                return (1);
        if (a->slvr_num < b->slvr_num)
                return (-1);
        return (0);
}

enum slvr_lookup_ops {
	SLVR_LOOKUP_ADD,
	SLVR_LOOKUP_DEL,
	SLVR_LOOKUP_NONE
};

struct slvr_ref *
	slvr_lookup(uint16_t, struct bmap_iod_info *, int);
void	slvr_cache_init(void);
int	slvr_do_crc(struct slvr_ref *);
int	slvr_fsbytes_io(struct slvr_ref *, int);
int	slvr_fsbytes_wio(struct slvr_ref *, uint32_t, uint32_t);
int	slvr_io_prep(struct slvr_ref *, uint32_t, uint32_t, int);
void	slvr_release(struct slvr_ref *);
void	slvr_rio_done(struct slvr_ref *);
void	slvr_slab_prep(struct slvr_ref *, int);
void	slvr_update(struct slvr_ref *);
void	slvr_wio_done(struct slvr_ref *);
void    slvr_try_rpcqueue(struct slvr_ref *);
void    slvr_worker_init(void);

#define slvr_io_done(s, rw) \
	((rw) == SL_WRITE ? slvr_wio_done(s) : slvr_rio_done(s))

#endif
