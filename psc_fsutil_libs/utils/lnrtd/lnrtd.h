/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2011, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _LNRTD_H_
#define _LNRTD_H_

enum {
	LRTHRT_CTL,
	LRTHRT_CTLAC,
	LRTHRT_EQPOLL,
	LRTHRT_LNETAC,
	LRTHRT_TIOS,
	LRTHRT_USKLNDPL
};

const char *ctlsockfn;

#endif /* _LNRTD_H_ */
