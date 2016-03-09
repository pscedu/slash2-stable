/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * --------------------------------------------------------------------
 * %END_LICENSE%
 */

#ifndef _PFL_FMTSTR_H_
#define _PFL_FMTSTR_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRFMTSTRCASEV(ch, code)						\
	case ch: {							\
		code;							\
		break;							\
	    }

#define PRFMTSTRCASE(ch, convfmt, ...)					\
	case ch:							\
		psc_assert(_t - _p + strlen(convfmt) + 1 <=		\
		    sizeof(_convbuf));					\
		_twant = snprintf(_convbuf, sizeof(_convbuf),		\
		    "%.*s%s", (int)(_t - _p), _p, convfmt);		\
		if (_twant == -1)					\
			break;						\
		fprintf(_fp, _convbuf, ## __VA_ARGS__);			\
		break;

#define PRFMTSTR(fp, fmt, cases)					\
	_PFL_RVSTART {							\
		char _convbuf[16];					\
		int _want = 0, _twant, _sawch;				\
		const char *_p, *_t;					\
		FILE *_fp = (fp);					\
									\
		for (_p = (fmt); *_p != '\0'; _p++) {			\
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
				PRFMTSTRCASE('%', "s%c", *_t == '%' ?	\
				    "" : "%", *_t)			\
				}					\
				if (_twant == -1) {			\
					_want = _twant;			\
					break;				\
				}					\
				_p = _t;				\
			} else {					\
				/*					\
				 * Not a special character;		\
				 * copy verbatim.			\
				 */					\
				fputc(*_p, _fp);			\
			}						\
		}							\
		_want;							\
	} _PFL_RVEND

#define FMTSTRCASE(ch, convfmt, ...)					\
	case ch:							\
		_tlen = _t - _p + strlen(convfmt) + 1;			\
		psc_assert(_tlen <= sizeof(_convbuf));			\
		/* write temporary buf for conversion specifier */	\
		_twant = snprintf(_convbuf, sizeof(_convbuf),		\
		    "%.*s%s", (int)(_t - _p), _p, convfmt);		\
		if (_twant == -1)					\
			break;						\
		/* convbuf is OK, use it to produce fmtstr atom now */	\
		_twant = snprintf(_s, _endt - _s, _convbuf,		\
		    ## __VA_ARGS__);					\
		break;

#define FMTSTR(buf, siz, fmt, cases)					\
	_PFL_RVSTART {							\
		char _convbuf[16], *_s, *_endt;				\
		int _want = 0, _twant, _sawch;				\
		const char *_p, *_t;					\
		size_t _tlen;						\
									\
		_s = (buf);						\
		_endt = _s + (siz);					\
		for (_p = (fmt); *_p != '\0'; _p++) {			\
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
				FMTSTRCASE('%', "s%c", *_t == '%' ?	\
				    "" : "%", *_t)			\
				}					\
				if (_twant == -1) {			\
					_want = _twant;			\
					break;				\
				}					\
				_want += _twant;			\
				if (_s + _twant > _endt)		\
					_s = _endt - 1;			\
				else					\
					_s += _twant;			\
				_p = _t;				\
			} else {					\
				/*					\
				 * Not a special character;		\
				 * copy verbatim.			\
				 */					\
				if (_s + 1 < _endt)			\
					*_s++ = *_p;			\
			}						\
		}							\
		/*							\
		 * Ensure NUL termination since we			\
		 * write some characters ourselves.			\
		 */							\
		if ((siz) > 0)						\
			*_s = '\0';					\
		_want;							\
	} _PFL_RVEND

#endif /* _PFL_FMTSTR_H_ */
