/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2010, Pittsburgh Supercomputing Center (PSC).
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psc_util/alloc.h"

extern __thread char	*pfl_fmtstr_buf;
extern __thread size_t	 pfl_fmtstr_len;

#define FMTSTRCASE(ch, buf, siz, convfmt, ...)				\
	case ch:							\
		_tlen = _t - _p + strlen(convfmt) + 1;			\
		if (_tlen > pfl_fmtstr_len && (_tfmt_new =		\
		    psc_realloc(pfl_fmtstr_buf, _tlen,			\
		    PAF_CANFAIL | PAF_NOLOG)) == NULL)			\
			_twant = -1;					\
		else {							\
			pfl_fmtstr_buf = _tfmt_new;			\
			pfl_fmtstr_len = _tlen;				\
									\
			_twant = snprintf(pfl_fmtstr_buf, _tlen,	\
			    "%.*s%s", (int)(_t - _p), _p, convfmt);	\
			if (_twant != -1)				\
				_twant = snprintf(_s, buf + siz - _s,	\
				    pfl_fmtstr_buf, ## __VA_ARGS__);	\
		}							\
		break;

#define FMTSTR(buf, siz, fmt, cases)					\
	({								\
		int _want, _twant, _sawch;				\
		char *_s, *_tfmt_new;					\
		const char *_p, *_t;					\
		size_t _tlen;						\
									\
		_s = buf;						\
		_tfmt_new = NULL;					\
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
		_want;							\
	})
