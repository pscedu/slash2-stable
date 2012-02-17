/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2005-2012, Pittsburgh Supercomputing Center (PSC).
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

/*
 * Simple routines for manipulating variable-sized
 * buffers.
 *
 * TODO: add the following members:
 *	- buf_opts with option BUFOPT_FATAL_ALLOC
 *	- buf_growamt to control growth size
 */

#ifndef _PFL_BUF_H_
#define _PFL_BUF_H_

struct buf {
	int	 buf_pos;
	int	 buf_max;
	char	*buf_buf;
};

#define buf_nul(buf) buf_append((buf), '\0')

void	 buf_append(struct buf *, int);
void	 buf_appendfv(struct buf *, const char *, ...);
void	 buf_appendv(struct buf *, const char *);
void	 buf_chop(struct buf *);
void	 buf_free(struct buf *);
char	*buf_get(const struct buf *);
void	 buf_init(struct buf *);
int	 buf_len(const struct buf *);
void	 buf_realloc(struct buf *);
void	 buf_reset(struct buf *);

#endif /* _PFL_BUF_H_ */
