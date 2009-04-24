/* $Id$ */

#ifndef _PFL_TIMERTHR_H_
#define _PFL_TIMERTHR_H_

void	*psc_timer_iosthr_main(void *);
void	 psc_timerthr_spawn(int, const char *);

extern struct psc_waitq psc_timerwtq;

#endif /* _PFL_TIMERTHR_H_ */
