/* $Id: assert.h 426 2006-12-07 17:08:43Z yanovich $ */

#if (!defined HAVE_PSC_ASSERT_INC)
#define HAVE_PSC_ASSERT_INC 1

#include "psc_util/log.h"

#define psc_assert(cond)				\
	do {						\
		if (!(cond))				\
			psc_fatalx("[assert] " # cond);	\
	} while (0)

#define psc_assert_msg(cond, format, ...)		\
	do {						\
		if (!(cond))				\
			psc_fatalx("[assert] " format,	\
			    ## __VA_ARGS__);		\
	} while (0)

#define psc_assert_perror(cond)			\
	do {						\
		if (!(cond))				\
			psc_fatal("[assert] " # cond);	\
	} while (0)

#endif
