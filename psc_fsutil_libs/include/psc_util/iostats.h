/* $Id$ */

#ifndef _IOSTATS_H_
#define _IOSTATS_H_

#include <sys/time.h>

#include <stdarg.h>

#include "zestAtomic.h"
#include "zestList.h"
#include "zestLock.h"
#include "zestTypes.h"

#define IST_NAME_MAX 30

struct iostats {
	char			ist_name[IST_NAME_MAX];
	struct psclist_head	ist_lentry;

	struct timeval		ist_lasttv;
	struct timeval		ist_intv;		/* collection period */

	atomic_t		ist_bytes_intv;
	u64			ist_bytes_total;
	double 			ist_rate;

	atomic_t		ist_errors_intv;
	u64			ist_errors_total;
	double			ist_erate;
};

extern psc_spinlock_t		iostatsListLock;
extern struct psclist_head	iostatsList;

void iostats_init(struct iostats *ist, const char *fmt, ...);

#endif /* _IOSTATS_H_ */
