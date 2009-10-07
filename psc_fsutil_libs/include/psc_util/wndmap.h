/* $Id$ */

#ifndef _PFL_WNDMAP_H_
#define _PFL_WNDMAP_H_

struct psc_wndmap {
	size_t			 pwm_min;	/* bottom edge of window */
	psc_spinlock_t		 pwm_lock;
	struct psclist_head	*pwm_wmbs;
};

struct psc_wndmap_block {
	struct psclist_head	 pwmb_lentry;
	char			 pwmb_buf[32];
};

#define WMBSZ (sizeof(((struct psc_wndmap_block *)NULL)->pwmb_buf) * NBBY)

struct psc_wndmap_block *
	psc_wndmap_addblock(struct psc_wndmap *);
int	psc_wndmap_block_full(struct psc_wndmap_block *);
void	psc_wndmap_free(struct psc_wndmap *);
void	psc_wndmap_init(struct psc_wndmap *, size_t);
int	psc_wndmap_set(struct psc_wndmap *, size_t);

#endif /* _PFL_WNDMAP_H_ */
