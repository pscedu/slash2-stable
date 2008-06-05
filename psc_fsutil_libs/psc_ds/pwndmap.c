/* $Id$ */

struct psc_wndmap {
	size_t			 pwm_min;	/* bottom edge of window */
	psc_spinlock_t		 pwm_lock;
	struct psclist_head	*pwm_wmbs;
};

/* windowmap block */
struct psc_wndmap_block {
	struct psclist_head	 pwmb_lentry;
	char			 pwmb_buf[32];
};

struct psc_wndmap_block *
pwndmap_addblock(struct psc_windowmap *p)
{
	struct psc_wndmap_block *pb;

	pb = PSCALLOC(sizeof(*pb));
	psclist_xadd_tail(&pb->pwmb_lentry, &p->pwm_wmbs);
	return (pb);
}

int
pwndmap_block_full(struct psc_wndmap_block *pb)
{
	int n;

	for (n = 0; n < sizeof(*pb); n++)
		if (pb->pwmb_buf[n] != 0xff)
			return (0);
	return (1);
}

void
pwndmap_init(struct psc_windowmap *p, size_t min)
{
	p->pwm_min = min;
	LOCK_INIT(&p->pwm_lock);
	PSCLIST_HEAD_INIT(&p->pwm_wmbs);
	pwndmap_addblock(p);
}

#define WMBSZ (sizeof(((struct psc_wndmap_block *)NULL)->pwmb_buf) * NBBY)

int
pwndmap_set(struct psc_wndmap *p, size_t pos)
{
	struct psc_wndmap_block *pb;
	int rc, nwrapend;

	rc = 0;
	spinlock(&p->pmw_lock);
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
				if (pwndmap_block_full(pb)) {
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
		pb = pwndmap_addblock(p);
	pb->pwmb_buf[pos / NBBY] |= 1 << (pos % NBBY - 1);
 done:
	freelock(&p->pmw_lock);
	return (rc);
}

void
pwndmap_free(struct psc_windowmap *p)
{
	struct psc_wndmap_block *pb, *nextpb;

	spinlock(&p->pwm_lock);
	psclist_for_each_entry_safe(pb,
	    nextpb, &p->pwm_wmbs, pwmb_lentry)
		free(pb);
	freelock(&p->pwm_lock);
}
