/* $Id$ */

#ifndef _PFL_IOSTATS_H_
#define _PFL_IOSTATS_H_

#include <sys/time.h>

#include <stdarg.h>

#include "psc_util/atomic.h"
#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_types.h"

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

extern psc_spinlock_t		pscIostatsListLock;
extern struct psclist_head	pscIostatsList;

void iostats_init(struct iostats *ist, const char *fmt, ...);

#endif /* _PFL_IOSTATS_H_ */
