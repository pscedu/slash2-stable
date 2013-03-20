/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2013, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#ifndef _PFL_FMTSTR_H_
#define _PFL_FMTSTR_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
		int _want, _twant, _sawch;				\
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
