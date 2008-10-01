/* $Id$ */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "psc_ds/list.h"
#include "psc_util/acsvc.h"
#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_util/cdefs.h"
#include "psc_util/strlcpy.h"
#include "psc_util/thread.h"
#include "psc_util/waitq.h"

struct access_request {
	int			 arq_id;
	uid_t			 arq_uid;
	gid_t			 arq_gid;
	int			 arq_op;
	char			 arq_fn[PATH_MAX];
	union {
		struct {
			mode_t mode;
		} arqdu_chmod;
		struct {
			uid_t uid;
			gid_t gid;
		} arqdu_chown;
		struct {
			char to[PATH_MAX];
		} arqdu_link;
		struct {
			mode_t mode;
		} arqdu_mkdir;
		struct {
			mode_t mode;
			dev_t dev;
		} arqdu_mknod;
		struct {
			int flags;
			mode_t mode;
		} arqdu_open;
		struct {
			size_t len;
		} arqdu_truncate;
		struct {
			struct timeval tv[2];
		} arqdu_utimes;
	} arq_datau;
#define arq_data_chmod		arq_datau.arqdu_chmod
#define arq_data_chown		arq_datau.arqdu_chown
#define arq_data_link		arq_datau.arqdu_link
#define arq_data_mkdir		arq_datau.arqdu_mkdir
#define arq_data_mknod		arq_datau.arqdu_mknod
#define arq_data_open		arq_datau.arqdu_open
#define arq_data_rename		arq_datau.arqdu_link
#define arq_data_symlink	arq_datau.arqdu_link
#define arq_data_truncate	arq_datau.arqdu_truncate
#define arq_data_utimes		arq_datau.arqdu_utimes
};

/* XXX opendir() needs access checks! */

struct access_reply {
	int			arp_op;
	int			arp_rc;
	int			arp_id;
	union {
		struct {
			int fd;
		} arpdu_open;
		struct {
			char fn[PATH_MAX];
		} arpdu_readlink;
		struct {
			struct stat sb;
		} arpdu_stat;
		struct {
			struct statvfs sv;
		} arpdu_statfs;
	} arp_datau;
#define arp_data_open		arp_datau.arpdu_open
#define arp_data_readlink	arp_datau.arpdu_readlink
#define arp_data_stat		arp_datau.arpdu_stat
#define arp_data_statfs		arp_datau.arpdu_statfs
};

struct access_pendreq {
	struct psclist_head	 apr_lentry;
	struct psc_wait_queue	 apr_wq;
	int			 apr_id;
	int			 apr_fd;
	int			 apr_op;
	struct access_reply	 apr_rep;
};

union access_ctlmsg {
	struct cmsghdr	ac_hdr;
	unsigned char	ac_buf[CMSG_SPACE(sizeof(int))];
};

__static int acsvc_fd;
__static atomic_t acsvc_id = ATOMIC_INIT(0);

__static psc_spinlock_t acsvc_pendlistlock = LOCK_INITIALIZER;
__static struct psclist_head acsvc_pendlist = PSCLIST_HEAD_INIT(acsvc_pendlist);

__static __dead void
acsvc_svrmain(int s)
{
	struct access_request arq;
	struct access_reply arp;
	union access_ctlmsg ac;
	struct cmsghdr *c;
	struct iovec iov;
	struct msghdr m;
	ssize_t nbytes;
	int fd, rc;

	fd = -1;
	for (;;) {
		/* Receive request. */
		memset(&m, 0, sizeof(m));
		iov.iov_base = &arq;
		iov.iov_len = sizeof(arq);
		m.msg_iov = &iov;
		m.msg_iovlen = 1;
 restart:
		nbytes = recvmsg(s, &m, 0); /* check TRUNC */
		if (nbytes == -1) {
			if (errno == EINTR)
				goto restart;
			psc_fatal("recvmsg");
		} else if (nbytes == 0)
			exit(0);
		else if (nbytes != sizeof(arq))
			psc_fatalx("recvmsg: short I/O (%zd)", nbytes);

		/* Setup reply. */
		memset(&m, 0, sizeof(m));
		memset(&arp, 0, sizeof(arp));
		iov.iov_base = &arp;
		iov.iov_len = sizeof(arp);
		m.msg_iov = &iov;
		m.msg_iovlen = 1;

		/* Invoke access operation. */
		if (seteuid(arq.arq_uid) == -1)
			psc_fatal("seteuid %d", arq.arq_uid);
//		if (setegid(arq.arq_gid) == -1)
//			psc_fatal("setegid %d", arq.arq_gid);
		switch (arq.arq_op) {
		case ACSOP_CHMOD:
			rc = chmod(arq.arq_fn, arq.arq_data_chmod.mode);
			break;
		case ACSOP_CHOWN:
			rc = chown(arq.arq_fn, arq.arq_data_chown.uid,
			    arq.arq_data_chown.gid);
			break;
		case ACSOP_LINK:
			rc = link(arq.arq_fn, arq.arq_data_link.to);
			break;
		case ACSOP_MKDIR:
			rc = mkdir(arq.arq_fn, arq.arq_data_mkdir.mode);
			break;
		case ACSOP_MKNOD:
			rc = mknod(arq.arq_fn, arq.arq_data_mknod.mode,
			    arq.arq_data_mknod.dev);
			break;
		case ACSOP_OPEN:
			arp.arp_data_open.fd = rc = fd = open(arq.arq_fn,
			    arq.arq_data_open.flags,
			    arq.arq_data_open.mode);
			if (fd != -1) {
				m.msg_control = ac.ac_buf;
				m.msg_controllen = sizeof(ac.ac_buf);

				c = CMSG_FIRSTHDR(&m);
				c->cmsg_len = CMSG_LEN(sizeof(int));
				c->cmsg_level = SOL_SOCKET;
				c->cmsg_type = SCM_RIGHTS;
				*(int *)CMSG_DATA(c) = fd;
			}
			break;
		case ACSOP_READLINK:
			rc = readlink(arq.arq_fn,
			    arp.arp_data_readlink.fn,
			    sizeof(arp.arp_data_readlink.fn));
			break;
		case ACSOP_RENAME:
			rc = rename(arq.arq_fn, arq.arq_data_rename.to);
			break;
		case ACSOP_RMDIR:
			rc = rmdir(arq.arq_fn);
			break;
		case ACSOP_STATFS:
			rc = statvfs(arq.arq_fn,
			    &arp.arp_data_statfs.sv);
			break;
		case ACSOP_SYMLINK:
			rc = symlink(arq.arq_fn,
			    arq.arq_data_symlink.to);
			break;
		case ACSOP_TRUNCATE:
			rc = truncate(arq.arq_fn,
			    arq.arq_data_truncate.len);
			break;
		case ACSOP_UNLINK:
			rc = unlink(arq.arq_fn);
			break;
		case ACSOP_UTIMES:
			rc = utimes(arq.arq_fn, arq.arq_data_utimes.tv);
			break;
		}
		arp.arp_id = arq.arq_id;
		arp.arp_op = arq.arq_op;
		if (rc == -1)
			arp.arp_rc = errno;

		/* Send reply. */
		nbytes = sendmsg(s, &m, 0);
		if (nbytes == -1)
			psc_fatal("sendmsg");
		else if (nbytes != sizeof(arp))
			psc_fatalx("sendmsg: short I/O, want %zu got %zd",
			    sizeof(arp), nbytes);

		/* Cleanup any work open(2) did. */
		if (fd != -1) {
			close(fd);
			fd = -1;
		}
	}
}

__dead void *
acsvc_climain(__unusedx void *arg)
{
	struct access_pendreq *apr;
	struct access_reply arp;
	union access_ctlmsg ac;
	struct cmsghdr *c;
	struct iovec iov;
	struct msghdr m;
	ssize_t nbytes;

	for (;;) {
		/* Receive a reply message from the child. */
		memset(&m, 0, sizeof(m));
		iov.iov_base = &arp;
		iov.iov_len = sizeof(arp);
		m.msg_iov = &iov;
		m.msg_iovlen = 1;
		m.msg_control = ac.ac_buf;
		m.msg_controllen = sizeof(ac.ac_buf);
		nbytes = recvmsg(acsvc_fd, &m, 0);
		if (nbytes == -1)
			psc_fatal("recvmsg");
		else if (nbytes != sizeof(arp))
			psc_fatalx("recvmsg: short I/O, want %zu, got %zd",
			    sizeof(arp), nbytes);

		if (m.msg_flags & MSG_TRUNC || m.msg_flags & MSG_CTRUNC)
			psc_fatalx("recvmsg: received truncated message");

		/* Scan through our active requests for the corresponder. */
		spinlock(&acsvc_pendlistlock);
		psclist_for_each_entry(apr, &acsvc_pendlist, apr_lentry)
			if (apr->apr_id == arp.arp_id)
				break;
		freelock(&acsvc_pendlistlock);

		if (apr == NULL)
			psc_fatalx("received a bogus reply");

		for (c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
			if (c->cmsg_len == CMSG_LEN(sizeof(int)) &&
			    c->cmsg_level == SOL_SOCKET &&
			    c->cmsg_type == SCM_RIGHTS) {
				if (apr->apr_op == ACSOP_OPEN &&
				    arp.arp_rc == 0)
					arp.arp_data_open.fd = *(int *)CMSG_DATA(c);
				break;
			}
		apr->apr_rep = arp;
		psc_waitq_wakeup(&apr->apr_wq);
	}
}

void
acsvc_init(struct psc_thread *thr, int thrtype, const char *name)
{
	int fds[2];

	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, fds) == -1)
		psc_fatal("socketpair");
	switch (fork()) {
	case -1:
		psc_fatal("fork");
	case 0:  /* child */
		close(fds[1]);
		acsvc_svrmain(fds[0]);
	}
	close(fds[0]);
	acsvc_fd = fds[1];

	pscthr_init(thr, thrtype, acsvc_climain, NULL, name);
}

__static struct access_request *
acsreq_new(int op, uid_t uid, gid_t gid)
{
	struct access_request *arq;

	arq = PSCALLOC(sizeof(*arq));
	arq->arq_op = op;
	arq->arq_uid = uid;
	arq->arq_gid = gid;
	arq->arq_id = atomic_inc_return(&acsvc_id);
	return (arq);
}

__static struct access_pendreq *
acsreq_issue(struct access_request *arq)
{
	struct access_pendreq *apr;
	struct iovec iov;
	struct msghdr m;
	int nbytes;

	memset(&m, 0, sizeof(m));
	iov.iov_base = arq;
	iov.iov_len = sizeof(*arq);
	m.msg_iov = &iov;
	m.msg_iovlen = 1;

	apr = PSCALLOC(sizeof(*apr));
	apr->apr_id = arq->arq_id;
	apr->apr_op = arq->arq_op;
	psc_waitq_init(&apr->apr_wq);

	spinlock(&acsvc_pendlistlock);
	psclist_xadd(&apr->apr_lentry, &acsvc_pendlist);
	freelock(&acsvc_pendlistlock);

	/* Issue request. */
	nbytes = sendmsg(acsvc_fd, &m, 0);
	if (nbytes == -1)
		psc_fatal("sendmsg");
	else if (nbytes != sizeof(*arq))
		psc_fatalx("sendmsg: short I/O");
	free(arq);

	/* Wait for request return. */
	psc_waitq_wait(&apr->apr_wq, NULL);	/* XXX check return */

	spinlock(&acsvc_pendlistlock);
	psclist_del(&apr->apr_lentry);
	freelock(&acsvc_pendlistlock);
	return (apr);
}

int
access_fsop(int op, uid_t uid, gid_t gid, const char *fn, ...)
{
	struct access_request *arq;
	struct access_pendreq *apr;
	va_list ap;
	int rc;

	/* Construct request. */
	arq = acsreq_new(op, uid, gid);
	if (strlcpy(arq->arq_fn, fn, PATH_MAX) >= PATH_MAX) {
		free(arq);
		errno = ENAMETOOLONG;
		return (-1);
	}
	va_start(ap, fn);
	switch (op) {
	case ACSOP_CHMOD:
		arq->arq_data_chmod.mode = va_arg(ap, mode_t);
		break;
	case ACSOP_CHOWN:
		arq->arq_data_chown.uid = va_arg(ap, uid_t);
		arq->arq_data_chown.gid = va_arg(ap, gid_t);
		break;
	case ACSOP_LINK:
		if (strlcpy(arq->arq_data_link.to, va_arg(ap, char *),
		    PATH_MAX) >= PATH_MAX)
			rc = ENAMETOOLONG;
		break;
	case ACSOP_MKDIR:
		arq->arq_data_mkdir.mode = va_arg(ap, mode_t);
		break;
	case ACSOP_MKNOD:
		arq->arq_data_mknod.mode = va_arg(ap, mode_t);
		arq->arq_data_mknod.dev = va_arg(ap, dev_t);
		break;
	case ACSOP_OPEN:
		arq->arq_data_open.flags = va_arg(ap, int);
		if (arq->arq_data_open.flags & O_CREAT)
			arq->arq_data_open.mode = va_arg(ap, mode_t);
		break;
	case ACSOP_RENAME:
		if (strlcpy(arq->arq_data_rename.to, va_arg(ap, char *),
		    PATH_MAX) >= PATH_MAX)
			rc = ENAMETOOLONG;
		break;
	case ACSOP_SYMLINK:
		if (strlcpy(arq->arq_data_symlink.to, va_arg(ap, char *),
		    PATH_MAX) >= PATH_MAX)
			rc = ENAMETOOLONG;
		break;
	case ACSOP_TRUNCATE:
		arq->arq_data_truncate.len = va_arg(ap, off_t);
		break;
	case ACSOP_UTIMES:
		memcpy(&arq->arq_data_utimes.tv,
		    va_arg(ap, struct timeval *),
		    sizeof(arq->arq_data_utimes.tv));
		break;
	default:
		psc_fatalx("unknown op: %d", op);
	}

	if (rc) {
		errno = rc;
		rc = -1;
	} else {
		apr = acsreq_issue(arq);
		switch (op) {
		case ACSOP_OPEN:
			if (apr->apr_rep.arp_data_open.fd == -1) {
				rc = -1;
				errno = apr->apr_rep.arp_rc;
			} else
				rc = apr->apr_rep.arp_data_open.fd;
			/* XXX: test validity of `fd', perhaps via select(2). */
			break;
//		case ACSOP_OPENDIR:
//			break;
		case ACSOP_READLINK:
			rc = apr->apr_rep.arp_rc;
			if (rc) {
				errno = rc;
				rc = -1;
			} else {
				char *buf;
				size_t len;

				buf = va_arg(ap, char *);
				len = va_arg(ap, size_t);
				if (len > PATH_MAX) {
					errno = EINVAL;
					rc = -1;
				} else {
					memcpy(buf,
					    apr->apr_rep.arp_data_readlink.fn,
					    len - 1);
					buf[len - 1] = '\0';
				}
			}
			break;
//		case ACSOP_STAT:
//			break;
		case ACSOP_STATFS:
			rc = apr->apr_rep.arp_rc;
			if (rc) {
				errno = rc;
				rc = -1;
			} else
				memcpy(va_arg(ap, struct statvfs *),
				    &apr->apr_rep.arp_data_statfs.sv,
				    sizeof(struct statvfs));
			break;
		default:
			rc = apr->apr_rep.arp_rc;
			if (rc) {
				errno = rc;
				rc = -1;
			}
			break;
		}
		free(apr); /* XXX might modify errno */
	}
	va_end(ap);
	return (rc);
}
