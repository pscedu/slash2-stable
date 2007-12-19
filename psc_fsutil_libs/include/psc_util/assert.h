/* $Id$ */

#ifndef _PFL_ASSERT_H_
#define _PFL_ASSERT_H_

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

#define psc_assert_perror(cond)				\
	do {						\
		if (!(cond))				\
			psc_fatal("[assert] " # cond);	\
	} while (0)

#endif /*_PFL_ASSERT_H_ */
