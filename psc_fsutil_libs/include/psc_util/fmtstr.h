/* $Id: fmtstr.h 1283 2007-07-17 16:43:48Z yanovich $ */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FMTSTRCASE(ch, buf, siz, convfmt, ...)				\
	case ch: {							\
		size_t _len;						\
									\
		_len = _t - _p + strlen(convfmt) + 1;			\
		if (_len > _tfmt_len && (_tfmt_new =			\
		    realloc(_tfmt, _len)) == NULL)			\
			_twant = -1;					\
		else {							\
			_tfmt = _tfmt_new;				\
			_tfmt_len = _len;				\
									\
			if ((_twant = snprintf(_tfmt, _len, "%.*s%s",	\
			    (int)(_t - _p), _p, convfmt) != -1))	\
				_twant = snprintf(_s, buf + siz - _s,	\
				    _tfmt, ## __VA_ARGS__);		\
		}							\
		break;							\
	}

#define FMTSTR(buf, siz, fmt, cases)					\
	({								\
		const char *_p, *_t;					\
		char *_s, *_tfmt, *_tfmt_new;				\
		int _want, _twant, _sawch;				\
		size_t _tfmt_len;					\
									\
		_s = buf;						\
		_tfmt = NULL;						\
		_tfmt_len = 0;						\
		for (_p = fmt; *_p != '\0'; _p++) {			\
		    	_sawch = 0;					\
			if (*_p == '%') {				\
				/* Look for a conversion specifier. */	\
				for (_t = _p + 1; *_t != '\0'; _t++) {	\
					if (isalpha(*_t)) {		\
						_sawch = 1;		\
						break;			\
					}				\
				}					\
			}						\
			if (_sawch) {					\
				switch (*_t) {				\
				cases					\
				/*					\
				 * Handle default (unknown) and `%%'	\
				 * cases with verbatim copying from	\
				 * the `invalid' custom format string.	\
				 */					\
				default:				\
				FMTSTRCASE('%', buf, siz, "s%c",	\
				    *_t == '%' ? "" : "%", *_t)		\
				}					\
				if (_twant == -1) {			\
					_want = _twant;			\
					break;				\
				}					\
				_want += _twant;			\
				if (_s + _twant > buf + siz)		\
					_s = buf + siz - 1;		\
				else					\
					_s += _twant;			\
				_p = _t;				\
			} else {					\
				/*					\
				 * Not a special character;		\
				 * copy verbatim.			\
				 */					\
				if (_s + 1 < buf + siz)			\
					*_s++ = *_p;			\
			}						\
		}							\
		/*							\
		 * Ensure NUL termination since we			\
		 * write some characters ourselves.			\
		 */							\
		if (siz > 0)						\
			*_s = '\0';					\
		free(_tfmt);						\
		_want;							\
	})
