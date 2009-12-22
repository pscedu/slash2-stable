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

	uint64_t		ist_len_total;			/* lifetime acculumator */

	struct psc_iostatv {
		struct timeval	istv_lastv;			/* time of last accumulation */
		psc_atomic64_t	istv_cur_len;			/* current accumulator */

		struct timeval	istv_intv_dur;			/* duration of accumulation */
		uint64_t	istv_intv_len;			/* length of accumulation */

	}			ist_intv[IST_NINTV];
};

#define psc_iostats_calcrate(len, tv)					\
	((len) / ((tv)->tv_sec * UINT64_C(1000000) + (tv)->tv_usec) * 1e-6)

#define psc_iostats_getintvrate(ist, n)					\
	psc_iostats_calcrate((ist)->ist_intv[n].istv_intv_len,		\
	    &(ist)->ist_intv[n].istv_intv_dur)

#define psc_iostats_getintvdur(ist, n)					\
	((ist)->ist_intv[n].istv_intv_dur.tv_sec +			\
	    (ist)->ist_intv[n].istv_intv_dur.tv_usec * 1e-6)

#define psc_iostats_intv_add(ist, amt)					\
	psc_atomic64_add(&(ist)->ist_intv[0].istv_cur_len, (amt))

#define psc_iostats_remove(ist)		pll_remove(&psc_iostats, (ist))

void psc_iostats_init(struct psc_iostats *, const char *, ...);
void psc_iostats_rename(struct psc_iostats *, const char *, ...);

extern struct psc_lockedlist	psc_iostats;
extern int			psc_iostat_intvs[];

#endif /* _PFL_IOSTATS_H_ */
