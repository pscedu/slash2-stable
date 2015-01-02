/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_IOSTATS_H_
#define _PFL_IOSTATS_H_

#include <sys/time.h>

#include <stdint.h>

#include "pfl/atomic.h"
#include "pfl/list.h"
#include "pfl/lockedlist.h"

#define IST_NAME_MAX	40
#define IST_NINTV	2

/*
 * XXX Need an instantaneous (accul over last second ) and an arbitrary
 * accumulator cleared by an external mechanism periodically.
 */

struct pfl_iostats {
	char			ist_name[IST_NAME_MAX];
	struct psclist_head	ist_lentry;
	int			ist_flags;

	uint64_t		ist_len_total;			/* lifetime acculumator */

	struct pfl_iostatv {
		struct timeval	istv_lastv;			/* time of last accumulation */
		psc_atomic64_t	istv_cur_len;			/* current accumulator */

		struct timeval	istv_intv_dur;			/* duration of accumulation */
		uint64_t	istv_intv_len;			/* length of accumulation */

	}			ist_intv[IST_NINTV];
};
#define psc_iostats pfl_iostats

#define PISTF_BASE10		(1 << 0)

struct pfl_iostats_rw {
	struct pfl_iostats	wr;
	struct pfl_iostats	rd;
};

/* graduated iostats */
struct pfl_iostats_grad {
	int64_t			size;
	struct pfl_iostats_rw	rw;
};

#define psc_iostats_calcrate(len, tv)					\
	((len) / (((tv)->tv_sec * UINT64_C(1000000) + (tv)->tv_usec) * 1e-6))

#define psc_iostats_getintvrate(ist, n)					\
	psc_iostats_calcrate((ist)->ist_intv[n].istv_intv_len,		\
	    &(ist)->ist_intv[n].istv_intv_dur)

#define psc_iostats_getintvdur(ist, n)					\
	((ist)->ist_intv[n].istv_intv_dur.tv_sec +			\
	    (ist)->ist_intv[n].istv_intv_dur.tv_usec * 1e-6)

#define psc_iostats_intv_add(ist, amt)					\
	psc_atomic64_add(&(ist)->ist_intv[0].istv_cur_len, (amt))

#define psc_iostats_remove(ist)		pll_remove(&psc_iostats, (ist))

#define psc_iostats_init(ist, name, ...)				\
	psc_iostats_initf((ist), 0, (name), ## __VA_ARGS__)

void psc_iostats_destroy(struct pfl_iostats *);
void psc_iostats_initf(struct pfl_iostats *, int, const char *, ...);
void psc_iostats_rename(struct pfl_iostats *, const char *, ...);

void pfl_iostats_grad_init(struct pfl_iostats_grad *, int, const char *);

extern struct psc_lockedlist	psc_iostats;
extern int			psc_iostat_intvs[];

#endif /* _PFL_IOSTATS_H_ */
