/* $Id$ */

#ifndef _PFL_IOSTATS_H_
#define _PFL_IOSTATS_H_

#include <sys/time.h>

#include <stdint.h>

#include "psc_ds/list.h"
#include "psc_util/atomic.h"
#include "psc_util/lock.h"

#define IST_NAME_MAX 24

struct iostats {
	char			ist_name[IST_NAME_MAX];
	struct psclist_head	ist_lentry;

	struct timeval		ist_lasttv;
	struct timeval		ist_intv;		/* amt over collection period */

	atomic_t		ist_bytes_intv;
	uint64_t		ist_bytes_total;
	double			ist_rate;

	atomic_t		ist_errors_intv;
	uint64_t		ist_errors_total;
	double			ist_erate;
};

#define iostats_intv_add(ist, amt)	atomic_add((amt), &(ist)->ist_bytes_intv)

void iostats_init(struct iostats *, const char *, ...);
void iostats_rename(struct iostats *, const char *, ...);

extern struct psc_lockedlist	psc_iostats;

#endif /* _PFL_IOSTATS_H_ */
