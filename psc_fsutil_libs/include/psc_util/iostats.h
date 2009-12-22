/* $Id$ */

#ifndef _PFL_IOSTATS_H_
#define _PFL_IOSTATS_H_

#include <sys/time.h>

#include <stdint.h>

#include "psc_ds/list.h"
#include "psc_ds/lockedlist.h"
#include "psc_util/atomic.h"

#define IST_NAME_MAX	24
#define IST_NINTV	2

struct psc_iostats {
	char			ist_name[IST_NAME_MAX];
	struct psclist_head	ist_lentry;

	psc_atomic64_t		ist_len_total;			/* lifetime */

	struct psc_iostatv {
		struct timeval	istv_lastv;			/* time of last accumulation */
		struct timeval	istv_intv;			/* duration of accumulation */
		psc_atomic64_t	istv_len;			/* length of accumulation */
	}			ist_intv[IST_NINTV];
};

#define psc_iostats_calcrate(len, tv)					\
	((len) / ((tv)->tv_sec * UINT64_C(1000000) + (tv)->tv_usec) * 1e-6)

#define psc_iostats_remove(ist)	pll_remove(&psc_iostats, (ist))

void psc_iostats_init(struct psc_iostats *, const char *, ...);
void psc_iostats_intv_add(struct psc_iostats *, uint64_t);
void psc_iostats_rename(struct psc_iostats *, const char *, ...);

extern struct psc_lockedlist	psc_iostats;
extern int			psc_iostat_intvs[];

#endif /* _PFL_IOSTATS_H_ */
