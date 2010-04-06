/* $Id$ */

#ifndef _PFL_STR_H_
#define _PFL_STR_H_

#include <sys/types.h>

#include <string.h>

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *, size_t);
#endif

#endif /* _PFL_STR_H_ */
