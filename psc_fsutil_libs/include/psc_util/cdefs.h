/* $Id$ */

#include <sys/cdefs.h>

#define NENTRIES(t) (int)(sizeof(t) / sizeof(t[0]))

#ifndef offsetof
#define offsetof(s, e) ((size_t)&((s *)0)->e)
#endif

/* Define __GNUC_PREREQ__(maj, min). */
#ifndef __GNUC_PREREQ__
# ifdef __GNUC__
#  define __GNUC_PREREQ__(maj, min) \
        ((__GNUC__ > (maj)) || (__GNUC__ == (maj) && __GNUC_MINOR__ >= (min)))
# else
#  define __GNUC_PREREQ__(maj, min) 0
# endif
#endif

/* no-op __attribute if old/non gcc */
#if !__GNUC_PREREQ__(2, 5)
# define __attribute__(x)
#endif

#ifndef __dead
# define __dead		__attribute__((__noreturn__))
#endif

#ifndef __packed
# define __packed	__attribute__((__packed__))
#endif

#define __unusedx	__attribute__((__unused__))

#undef __weak
#define __weak		__attribute__((__weak__))

/*
 * Keyword to mark something as file-scoped
 * without the side effects of using `static'.
 */
#define __static

#define ATTR_HASALL(c, a)	(((c) & (a)) == (a))
#define ATTR_HASANY(c, a)	((c) & (a))
#define ATTR_TEST(c, a)		((c) & (a))
#define ATTR_SET(c, a)		((c) |= (a))
#define ATTR_ISSET(c, a)	ATTR_TEST((c), (a))
#define ATTR_UNSET(c, a)	((c) &= ~(a))
#define ATTR_RESET(c)		((c) = 0)
