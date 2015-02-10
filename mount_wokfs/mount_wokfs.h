/* $Id$ */
/* %PSC_COPYRIGHT% */

#ifndef _MOUNT_WOKFS_H_
#define _MOUNT_WOKFS_H_

enum {
	THRT_FS,
	THRT_CTLAC,
	THRT_CTL,
	THRT_FSMGR,
	THRT_OPSTIMER
};

struct wokfs_thread {
	size_t			 ft_uniqid;
	struct pscfs_req	*ft_pfr;
};

#define PATH_CTLSOCK "/var/run/mount_wokfs.%h.sock"

void ctlthr_spawn(void);

extern const char		*ctlsockfn;
extern char			 mountpoint[];

#endif /* _MOUNT_WOKFS_H_ */
