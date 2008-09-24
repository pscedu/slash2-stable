/* $Id$ */

#define NENTRIES(t) (int)(sizeof(t) / sizeof(t[0]))

#ifndef offsetof
#define offsetof(s, e) ((size_t)&((s *)0)->e)
#endif

#ifndef __dead
#define __dead		__attribute__((__noreturn__))
#endif

#ifndef __packed
#define __packed	__attribute__((__packed__))
#endif

#define __unusedx	__attribute__((__unused__))

#ifdef __weak
#undef __weak
#endif

#define __weak		__attribute__((__weak__))

#ifdef __cplusplus
# define __BEGIN_DECLS	extern "C" {
# define __END_DECLS	}
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

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
