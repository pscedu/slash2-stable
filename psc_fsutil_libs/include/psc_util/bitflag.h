#ifndef __BITFLAG_H_
#define __BITFLAG_H_ 1

#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

#define BIT_CHK           (1<<0)
#define BIT_SET           (1<<1)
#define BIT_STRICT        (1<<2)
#define BIT_CHK_STRICT    (BIT_CHK | BIT_STRICT)
#define BIT_SET_STRICT    (BIT_SET | BIT_STRICT)
#define BIT_CHKSET        (BIT_CHK | BIT_SET)
#define BIT_CHKSET_STRICT (BIT_CHKSET | BIT_STRICT)
#define BIT_ABORT         (1<<31)

/* 
 * Notes on usage, based on the value of 'sorc':
 * BIT_CHKSET means:
 *   if any "on" bits were on, set all "off" bits on
 * BIT_CHECKSET | BIT_STRICT means:
 *   if all "on" bits were on, then make sure "off" bits were off
 *   and set the "off" bits on
 * BIT_CHK means:
 *   check that at least some "on" bits were on,
 *   and that at least some "off" bits were off
 * BIT_CHK | BIT_STRICT means:
 *   check that all "on" bits were on,
 *   and that all "off" bits were off
 * BIT_SET means:
 *   set all "on" bits on
 *   set all "off" bits off
 * BIT_SET | BIT_STRICT means:
 *   make sure allo "on" bits were off, and that all "off" bits were off
 *   then set all "on" bits on and all "off" bits off
 */

static inline int
bitflag_sorc(int *f, psc_spinlock_t *lck, int on, int off, int sorc)
{
        int rc=0, l;
	if (lck)
		l = reqlock(lck);

	if (!(sorc & BIT_CHKSET)) /* gotta be one, the other, or both */
		abort();

	if (ATTR_HASALL(sorc, BIT_CHKSET)){
		/* Alternate mode where if 'ons' then
		 *  enable the 'offs'.
		 */
		if ((sorc & BIT_STRICT) &&
		    on && off && ATTR_HASANY(*f, off)) {
			rc = -1; /* error, because the offs
				  * were already set */
			goto done;
		}

		/* In chkset mode no flags are disabled, 
		 *  the off's are turned on (conditionally)
		 */
		if (ATTR_HASALL(*f, on) ||
		    (!(sorc & BIT_STRICT) && ATTR_HASANY(*f, on)))
			ATTR_SET(*f, off);
		rc = 0;
		goto done;
	}

        if (sorc & BIT_CHK) {
                if ((!off || !ATTR_HASANY(*f, off)) &&  /* None of the 'offs' AND  */
		    ((!on || ATTR_HASALL(*f, on)) ||   /* Have all of the 'ons' OR */
		     (!(sorc & BIT_STRICT) && /* not strict.. AND  */
		      ((!on || ATTR_HASANY(*f, on)) &&   /* Have any of the 'ons' AND */
		       (!off || ATTR_HASANY(~(*f), off)))))) /* any of the 'offs' were off */
                        rc = 0;
		else {
			rc = -1;
		}
        }

	if (sorc & BIT_SET) {
                if (sorc & BIT_STRICT) {
			/* 'Normal' mode, where 'ons' are enabled
			 *   and 'offs' are disabled.
			 */
			if ((on && ATTR_HASANY(*f, on)) || 
			    (off && !ATTR_HASALL(*f, off))) {
				rc = -1; /* error, because they were
					  * already in the end state */
				goto done;
			}
		}
		ATTR_SET(*f, on);
		ATTR_UNSET(*f, off);
		rc = 0;
	}
 done:
	if (rc && (sorc & BIT_ABORT))
		abort();

	if (lck)
		ureqlock(lck, l);
        return (rc);
}

#endif
