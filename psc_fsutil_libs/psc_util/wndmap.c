/* $Id$ */

#include <sys/param.h>

#include <string.h>

#include "psc_ds/list.h"
#include "psc_util/alloc.h"
#include "psc_util/wndmap.h"
#include "psc_util/spinlock.h"

__static struct psc_wndmap_block *
psc_wndmap_addblock(struct psc_wndmap *p)
{
	struct psc_wndmap_block *pb;

	pb = PSCALLOC(sizeof(*pb));
	psclist_xadd_tail(&pb->pwmb_lentry, &p->pwm_wmbs);
	return (pb);
}

__static int
psc_wndmap_block_full(struct psc_wndmap_block *pb)
{
	int n;

	for (n = 0; n < sizeof(*pb); n++)
		if (pb->pwmb_buf[n] != 0xff)
			return (0);
	return (1);
}

void
psc_wndmap_init(struct psc_wndmap *p, size_t min)
{
	p->pwm_min = min;
	LOCK_INIT(&p->pwm_lock);
	PSCLIST_HEAD_INIT(&p->pwm_wmbs);
	psc_wndmap_addblock(p);
}

int
psc_wndmap_set(struct psc_wndmap *p, size_t pos)
{
	struct psc_wndmap_block *pb;
	int rc, nwrapend;

	rc = 0;
	WNDMAP_LOCK(p);
	if (pos < p->pwm_min) {
		nwrapend = 0;
		psclist_for_each_entry(pb, &p->pwm_wmbs, pwmb_lentry)
			if (++nwrapend * WMBSZ + p->pwm_min < p->pwm_min)
				break;
		if (pb == NULL) {
			rc = -1;
			goto done;
		}
		pos += WMBSZ * nwrapend;
	} else
		pos -= p->pwm_min;
	psclist_for_each_entry(pb, &p->pwm_wmbs, pwmb_lentry) {
		if (pos < WMBSZ) {
			if (pb->pwmb_buf[pos / NBBY] & (1 << (pos % NBBY - 1)))
				rc = 1;
			else {
				pb->pwmb_buf[pos / NBBY] |= 1 << (pos % NBBY - 1);
				if (psc_wndmap_block_full(pb)) {
					p->pwm_min += WMBSZ;

					psclist_del(&pb->pwmb_lentry)
					memset(pb, 0, sizeof(*pb));
					psclist_xadd_tail(&pb->pwmb_lentry, &p->pwm_wmbs);
				}
			}
			goto done;
		}
	}
	for (; pos >= WMBSZ; pos -= WMBSZ)
		pb = psc_wndmap_addblock(p);
	pb->pwmb_buf[pos / NBBY] |= 1 << (pos % NBBY - 1);
 done:
	WNDMAP_ULOCK(p);
	return (rc);
}

void
psc_wndmap_free(struct psc_wndmap *p)
{
	struct psc_wndmap_block *pb, *nextpb;

	WNDMAP_LOCK(p);
	psclist_for_each_entry_safe(pb,
	    nextpb, &p->pwm_wmbs, pwmb_lentry)
		PSCFREE(pb);
	WNDMAP_ULOCK(p);
}
