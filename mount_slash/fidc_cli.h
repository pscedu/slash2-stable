/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
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

#ifndef _FIDC_CLIENT_H_
#define _FIDC_CLIENT_H_

#include "psc_ds/list.h"
#include "psc_util/lock.h"

#include "fid.h"

struct fidc_membh;

struct fidc_nameinfo {
	int			 fni_hash;
	char			 fni_name[];
};

struct fidc_membh *fidc_child_lookup(struct fidc_membh *, const char *);

void	fidc_child_add(struct fidc_membh *, struct fidc_membh *, const char *);
int	fidc_child_reap_cb(struct fidc_membh *);
void	fidc_child_rename(struct fidc_membh *, const char *, struct fidc_membh *, const char *);
void	fidc_child_unlink(struct fidc_membh *, const char *);

#endif /* _FIDC_CLIENT_H_ */
