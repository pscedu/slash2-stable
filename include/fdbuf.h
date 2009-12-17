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

/*
 * fdbuf - file descriptor buffer routines.
 */

#ifndef _SL_FDBUF_H_
#define _SL_FDBUF_H_

#include "slconfig.h"

struct stat;
struct srt_fd_buf;
struct srt_bmapdesc_buf;

void bdbuf_sign(struct srt_bmapdesc_buf *, const struct slash_fidgen *,
	lnet_process_id_t, lnet_nid_t, sl_ios_id_t, sl_blkno_t);
int  bdbuf_check(const struct srt_bmapdesc_buf *, uint64_t *,
	struct slash_fidgen *, sl_blkno_t *, lnet_process_id_t, lnet_nid_t,
	sl_ios_id_t, enum rw);

void fdbuf_sign(struct srt_fd_buf *, const struct slash_fidgen *,
	lnet_process_id_t);
int  fdbuf_check(const struct srt_fd_buf *, uint64_t *,
	struct slash_fidgen *, lnet_process_id_t);
void fdbuf_readkeyfile(void);
void fdbuf_checkkeyfile(void);
void fdbuf_checkkey(const char *, struct stat *);
void fdbuf_createkeyfile(void);

#endif /* _SL_FDBUF_H_ */
