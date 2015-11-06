/* $Id$ */
/* %ISC_COPYRIGHT% */

#ifndef _MOUNT_WOKFS_CTL_H_
#define _MOUNT_WOKFS_CTL_H_

struct wokctlmsg_modspec {
	char			wcms_path[PATH_MAX];
	int32_t			wcms_pos;
};

struct wokctlmsg_modctl {
	int32_t			wcmc_pos;
};

#define PATH_CTLSOCK		"/var/run/mount_wokfs.%h.sock"

#define WOKCMT_INSERT		NPCMT
#define WOKCMT_LIST		(NPCMT + 1)
#define WOKCMT_RELOAD		(NPCMT + 2)
#define WOKCMT_REMOVE		(NPCMT + 3)

void ctlthr_spawn(void);

extern const char		*ctlsockfn;

#endif /* _MOUNT_WOKFS_CTL_H_ */
