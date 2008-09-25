/* $Id$ */

#ifndef __PFL_BITFLAG_H__
#define __PFL_BITFLAG_H__

#include "psc_util/assert.h"
#include "psc_util/cdefs.h"
#include "psc_util/lock.h"

#define BIT_STRICT		(1 << 0)
#define BIT_ABORT		(1 << 1)

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

	if (lck)
		locked = reqlock(lck);

	/* check on bits */
	if (checkon &&
	    (!ATTR_HASANY(*f, checkon) ||
	     (ATTR_ISSET(flags, BIT_STRICT) &&
	      !ATTR_HASALL(*f, checkon))))
		goto error;
	
	/* check off bits */
	if (checkoff &&
	    (ATTR_HASALL(*f, checkoff) ||
	     (ATTR_ISSET(flags, BIT_STRICT) &&
	      ATTR_HASANY(*f, checkoff))))
		goto error;

	/* set on bits */
	if (turnon &&
	    (ATTR_ISSET(flags, BIT_STRICT) &&
	     ATTR_HASANY(*f, turnon)))
		goto error;
	else
		*f |= turnon;

	/* unset off bits */
	if (turnoff &&
	    (ATTR_ISSET(flags, BIT_STRICT) &&
	     ATTR_HASANY(~(*f), turnoff)))
		goto error;
	else
		*f &= ~turnoff;

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
