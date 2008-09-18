#ifndef __BITFLAG_H_
#define __BITFLAG_H_ 1

#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

#define BIT_CHK           (1<<0)
#define BIT_CHK_STRICT    (1<<1 | BIT_CHK)
#define BIT_SET           (1<<2)
#define BIT_SET_STRICT    (1<<3 | BIT_SET)
#define BIT_CHKSET        (BIT_CHK | BIT_SET)
#define BIT_CHKSET_STRICT (BIT_CHK_STRICT | BIT_SET_STRICT)
#define BITFLAG_ABORT     (1<<31)

static inline int
bitflag_sorc(int *f, psc_spinlock_t *lck, int on, int off, int sorc)
{
        int rc=0, l;
	if (lck)
		l = reqlock(lck);

	if (!(sorc & BIT_CHK || sorc & BIT_SET))
		abort();

        if (sorc & BIT_CHK) {
                if (!ATTR_HASANY(*f, off) &&  /* None of the 'offs' AND  */
		    (ATTR_HASALL(*f, on) ||   /* Have all of the 'ons' OR */
		     ((sorc != BIT_CHK_STRICT) && /* not strict.. AND *   */
		      ATTR_HASANY(*f, on))))  /* Have any of the 'ons'    */
                        rc = 0;
		else {
			rc = -1;
			goto done;
		}
        } 	
	if (sorc & BIT_SET) {
                if (sorc & BIT_SET_STRICT) {
			if ((sorc & BIT_CHKSET) != BIT_CHKSET) {
				/* 'Normal' mode, where 'ons' are enabled
				 *   and 'offs' are disabled.
				 */
				if (ATTR_HASANY(*f, on) || 
				    !ATTR_HASALL(*f, off)) {
					rc = -1;
					goto done;
				}
			} else 
				/* Alternate mode where if 'ons' then
				 *  enable the 'offs'.
				 */
				if (ATTR_HASANY(*f, off)) {
					rc = -1;
					goto done;
				}
		}
		if ((sorc & BIT_CHKSET) != BIT_CHKSET) {
			ATTR_SET(*f, on);
			ATTR_UNSET(*f, off);
		} else
			/* In chkset mode no flags are disabled, 
			 *  the off's are turned on.
			 */
			ATTR_SET(*f, off);
		rc = 0;
	}
 done:
	if (rc && (sorc & BITFLAG_ABORT))
		abort();

	if (lck)
		ureqlock(lck, l);
        return (rc);
}

#endif
