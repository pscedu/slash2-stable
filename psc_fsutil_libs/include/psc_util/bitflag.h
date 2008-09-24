/* $Id$ */

#ifndef __PFL_BITFLAG_H__
#define __PFL_BITFLAG_H__

#include "psc_util/assert.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

#define BIT_CHK			(1 << 0)
#define BIT_SET			(1 << 1)
#define BIT_STRICT		(1 << 2)
#define BIT_ABORT		(1 << 3)

#define BIT_CHK_STRICT		(BIT_CHK | BIT_STRICT)
#define BIT_SET_STRICT		(BIT_SET | BIT_STRICT)
#define BIT_CHKSET		(BIT_CHK | BIT_SET)
#define BIT_CHKSET_STRICT	(BIT_CHK | BIT_SET | BIT_STRICT)

/*
 * bitflag_sorc - check and/or set flags on a variable.
 * @f: flags variable to perform operations on.
 * @lck: optional spinlock.
 * @checkon: values to ensure are enabled.
 * @checkoff: values to ensure are disabled.
 * @turnon: values to enable.
 * @turnoff: values to disable.
 * @flags: settings which dictate operation of this routine.
 * Notes: returns -1 on failure, 0 on success.
 */
static __inline int
bitflag_sorc(int *f, psc_spinlock_t *lck, int checkon, int checkoff,
    int turnon, int turnoff, int flags)
{
        int locked;

	psc_assert(ATTR_HASANY(flags, BIT_CHK | BIT_SET));

	if (lck)
		locked = reqlock(lck);

	if (flags & BIT_CHK) {
		psc_assert(checkon | checkoff);
		if (flags & BIT_STRICT) {
			if (!ATTR_HASALL(*f, checkon) ||
			    ATTR_HASANY(*f, checkoff))
				goto error;
		} else {
			if (!ATTR_HASANY(*f, checkon))
				goto error;
		}
	}
	if (flags & BIT_SET) {
		psc_assert(turnon | turnoff);
		if (flags & BIT_STRICT) {
			if (ATTR_HASANY(*f, turnon))
				goto error;
			if (!ATTR_HASALL(*f, turnoff))
				goto error;
		}
		*f |= turnon;
		*f &= ~turnoff;
	}
	if (lck)
		ureqlock(lck, locked);
	return (0);
 error:
	if (lck)
		ureqlock(lck, locked);
	psc_assert((flags & BIT_ABORT) == 0);
	return (-1);
}

#endif /* __PFL_BITFLAG_H__ */
