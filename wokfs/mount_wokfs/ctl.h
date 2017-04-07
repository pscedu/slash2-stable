/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015, Google, Inc.
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

#ifndef _MOUNT_WOKFS_CTL_H_
#define _MOUNT_WOKFS_CTL_H_

struct wokctlmsg_modspec {
	char			wcms_path[PATH_MAX];
	char			wcms_opts[LINE_MAX];
	int32_t			wcms_pos;
};

struct wokctlmsg_modctl {
	int32_t			wcmc_pos;
};

#define PATH_CTLSOCK		"/var/run/mount_wokfs.%h.sock"

/*
 * The order of this list must match ctlops[] in ctl.c.
 */
#define WOKCMT_INSERT		NPCMT
#define WOKCMT_LIST		(NPCMT + 1)
#define WOKCMT_RELOAD		(NPCMT + 2)
#define WOKCMT_REMOVE		(NPCMT + 3)

void ctlthr_spawn(void);

extern const char		*ctlsockfn;

#endif /* _MOUNT_WOKFS_CTL_H_ */
