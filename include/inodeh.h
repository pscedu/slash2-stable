/* $Id$ */

#ifndef _SLASH_INODEH_H_
#define _SLASH_INODEH_H_

#include <inttypes.h>

#include "psc_util/lock.h"

#include "inode.h"
#include "jflush.h"
#include "fidcache.h"

struct slash_inode_handle {
	struct slash_inode_od         inoh_ino;
	struct slash_inode_extras_od *inoh_extras;
	struct fidc_membh            *inoh_fcmh;
	psc_spinlock_t                inoh_lock;
	struct jflush_item            inoh_jfi;
	int                           inoh_flags;
};

#define INOH_LOCK(ih)		spinlock(&(ih)->inoh_lock)
#define INOH_ULOCK(ih)		freelock(&(ih)->inoh_lock)
#define INOH_RLOCK(ih)		reqlock(&(ih)->inoh_lock)
#define INOH_URLOCK(ih, lk)	ureqlock(&(ih)->inoh_lock, (lk))
#define INOH_LOCK_ENSURE(ih)	LOCK_ENSURE(&(ih)->inoh_lock)

enum slash_inode_handle_flags {
	INOH_INO_DIRTY     = (1<<0), /* Inode structures need to be written */
	INOH_EXTRAS_DIRTY  = (1<<1), /* Replication structures need written */
	INOH_HAVE_EXTRAS   = (1<<2),
	INOH_INO_NEW       = (1<<3), /* The inode info has never been written
					to disk */
	INOH_LOAD_EXTRAS   = (1<<4),
	INOH_INO_NOTLOADED = (1<<5),
};

static __inline void
slash_inode_handle_init(struct slash_inode_handle *i,
    struct fidc_membh *f, jflush_handler handler)
{
	i->inoh_fcmh = f;
	i->inoh_extras = NULL;
	LOCK_INIT(&i->inoh_lock);
	jfi_init(&i->inoh_jfi, handler, i);
	i->inoh_flags = INOH_INO_NOTLOADED;
}

#define FCMH_2_INODEP(f)	(&(f)->fcmh_memb.fcm_inodeh.inoh_ino)

#define DEBUG_INOH_FLAGS(i)						\
	(i)->inoh_flags & INOH_INO_DIRTY	? "D" : "",		\
	(i)->inoh_flags & INOH_EXTRAS_DIRTY	? "d" : "",		\
	(i)->inoh_flags & INOH_HAVE_EXTRAS	? "X" : "",		\
	(i)->inoh_flags & INOH_INO_NEW		? "N" : ""

#define INOH_FLAGS_FMT "%s%s%s%s"

#define DEBUG_INOH(level, i, fmt, ...)					\
	psc_logs((level), PSS_GEN,					\
		 " inoh@%p f:"FIDFMT" fl:"INOH_FLAGS_FMT		\
		 "v:%x bsz:%u nr:%u cs:%u "				\
		 "repl0:%u crc:%"PRIx64" :: "fmt,			\
		 (i), FIDFMTARGS(&(i)->inoh_ino.ino_fg),		\
		 DEBUG_INOH_FLAGS(i),					\
		 (i)->inoh_ino.ino_version, (i)->inoh_ino.ino_bsz,	\
		 (i)->inoh_ino.ino_nrepls, (i)->inoh_ino.ino_csnap,	\
		 (i)->inoh_ino.ino_repls[0].bs_id,			\
		 (i)->inoh_ino.ino_crc,					\
		 ## __VA_ARGS__)

#endif
