/* $Id$ */

#ifndef _PFL_WNDMAP_H_
#define _PFL_WNDMAP_H_

struct psc_wndmap {
	size_t			 pwm_min;	/* bottom edge of window */
	psc_spinlock_t		 pwm_lock;
	struct psc_lockedlist	*pmw_wmbs;
};

struct psc_wndmap_block {
	struct psclist_head	 pwmb_lentry;
	char			 pwmb_buf[32];
};

#define WNDMAP_LOCK(wm)		spinlock(&(wm)->pwm_lock)
#define WNDMAP_ULOCK(wm)	freelock(&(wm)->pwm_lock)
#define WNDMAP_RLOCK(wm)	reqlock(&(wm)->pwm_lock)
#define WNDMAP_URLOCK(wm, lk)	ureqlock(&(wm)->pwm_lock, (lk))

void	psc_wndmap_free(struct psc_wndmap *);
int	psc_wndmap_getnext(struct psc_wndmap *, size_t *);
void	psc_wndmap_init(struct psc_wndmap *, size_t);
int	psc_wndmap_setpos(struct psc_wndmap *, size_t);

#endif /* _PFL_WNDMAP_H_ */
