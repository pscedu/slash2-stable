/* $Id$ */
/*
 * %ISC_START_LICENSE%
 * ---------------------------------------------------------------------
 * Copyright 2015-2016, Google, Inc.
 * Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pfl/cdefs.h"
#include "pfl/pfl.h"
#include "pfl/str.h"
#include "pfl/list.h"
#include "pfl/acsvc.h"
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/thread.h"
#include "pfl/waitq.h"

struct access_request {
	int			 arq_id;
	uid_t			 arq_uid;
	gid_t			 arq_gid;
	int			 arq_op;
	char			 arq_fn[PATH_MAX];
	union {
		struct {
			int mode;
		} arqdu_access;
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
			off_t len;
		} arqdu_truncate;
		struct {
			struct timeval tv[2];
		} arqdu_utimes;
	} arq_datau;
#define arq_data_access		arq_datau.arqdu_access
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
			ssize_t len;
		} arpdu_readlink;
		struct {
			struct stat stb;
		} arpdu_stat;
		struct {
			struct statvfs sv;
		} arpdu_statfs;
	} arp_datau;
#define arp_data_open		arp_datau.arpdu_open
#define arp_data_readlink	arp_datau.arpdu_readlink
#define arp_data_stat		arp_datau.arpdu_stat
#define arp_data_lstat		arp_datau.arpdu_stat
#define arp_data_statfs		arp_datau.arpdu_statfs
};

struct access_pendreq {
	struct psclist_head	 apr_lentry;
	psc_spinlock_t		 apr_lock;
	struct psc_waitq	 apr_wq;
	int			 apr_id;
	int			 apr_fd;
	int			 apr_op;
	struct access_reply	 apr_rep;
};

union access_ctlmsg {
	struct cmsghdr		ac_hdr;
	unsigned char		ac_buf[CMSG_SPACE(sizeof(int))];
};

__static int		acsvc_fd;
__static psc_atomic32_t	acsvc_id = PSC_ATOMIC32_INIT(0);

__static struct psc_lockedlist acsvc_pendlist =
    PLL_INIT(&acsvc_pendlist, struct access_pendreq, apr_lentry);

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
	uid_t euid;
	gid_t egid;
	int fd, rc;

	euid = geteuid();
	egid = getegid();

	fd = -1;
	for (;;) {
		/* Receive request. */
		memset(&m, 0, sizeof(m));
		iov.iov_base = &arq;
		iov.iov_len = sizeof(arq);
		m.msg_iov = &iov;
		m.msg_iovlen = 1;
		nbytes = recvmsg(s, &m, MSG_WAITALL); /* check TRUNC */
		if (nbytes == -1) {
			if (errno == EINTR)
				continue;
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

		/*
		 * Looks like that a client does send an open request
		 * to us instead of doing it locally. So I have to do a
		 * chown() afterwards to correct the uid/gid setting.
		 */
		if (arq.arq_uid != euid || arq.arq_gid != egid) {
			if (seteuid(0) == -1)
				psc_fatal("seteuid 0");
			if (setegid(arq.arq_gid) == -1)
				psc_fatal("setegid %d", arq.arq_gid);
			egid = arq.arq_gid;
			if (seteuid(arq.arq_uid) == -1)
				psc_fatal("seteuid %d", arq.arq_uid);
			euid = arq.arq_uid;
		}

		/* Invoke access operation. */
		switch (arq.arq_op) {
		case ACSOP_ACCESS:
			rc = access(arq.arq_fn, arq.arq_data_access.mode);
			break;
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
		case ACSOP_LSTAT:
			rc = lstat(arq.arq_fn, &arp.arp_data_lstat.stb);
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
				int *p;

				m.msg_control = ac.ac_buf;
				m.msg_controllen = sizeof(ac.ac_buf);

				c = CMSG_FIRSTHDR(&m);
				c->cmsg_len = CMSG_LEN(sizeof(int));
				c->cmsg_level = SOL_SOCKET;
				c->cmsg_type = SCM_RIGHTS;

				/* workaround gcc aliasing complaint */
				p = (void *)CMSG_DATA(c);
				*p = fd;
			}
			break;
		case ACSOP_READLINK:
			rc = readlink(arq.arq_fn,
			    arp.arp_data_readlink.fn,
			    sizeof(arp.arp_data_readlink.fn));
			if (rc != -1)
				arp.arp_data_readlink.len = rc;
			break;
		case ACSOP_RENAME:
			rc = rename(arq.arq_fn, arq.arq_data_rename.to);
			break;
		case ACSOP_RMDIR:
			rc = rmdir(arq.arq_fn);
			break;
		case ACSOP_STAT:
			rc = stat(arq.arq_fn, &arp.arp_data_stat.stb);
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
		default:
			psc_fatal("unknown op %d", arq.arq_op);
		}
		arp.arp_id = arq.arq_id;
		arp.arp_op = arq.arq_op;
		if (rc == -1)
			arp.arp_rc = errno;

		/* Send reply. */
		nbytes = sendmsg(s, &m, MSG_WAITALL);
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

void
acsvc_climain(__unusedx struct psc_thread *thr)
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
		nbytes = recvmsg(acsvc_fd, &m, MSG_WAITALL);
		if (nbytes == -1) {
			if (errno == EINTR)
				continue;
			psc_fatal("recvmsg");
		} else if (nbytes == 0)
			psc_fatalx("recvmsg: unexpected EOF");
		else if (nbytes != sizeof(arp))
			psc_fatalx("recvmsg: short I/O, want %zu, got %zd",
			    sizeof(arp), nbytes);

		if (m.msg_flags & MSG_TRUNC || m.msg_flags & MSG_CTRUNC)
			psc_fatalx("recvmsg: received truncated message");

		/* Scan through our active requests for the corresponder. */
		PLL_LOCK(&acsvc_pendlist);
		PLL_FOREACH(apr, &acsvc_pendlist)
			if (apr->apr_id == arp.arp_id)
				break;
		PLL_ULOCK(&acsvc_pendlist);

		if (apr == NULL)
			psc_fatalx("received a bogus reply");

		for (c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
			if (c->cmsg_len == CMSG_LEN(sizeof(int)) &&
			    c->cmsg_level == SOL_SOCKET &&
			    c->cmsg_type == SCM_RIGHTS) {
				int *p;

				p = (void *)CMSG_DATA(c);
				if (apr->apr_op == ACSOP_OPEN &&
				    arp.arp_rc == 0)
					arp.arp_data_open.fd = *p;
				break;
			}
		apr->apr_rep = arp;
		spinlock(&apr->apr_lock);
		psc_waitq_wakeall(&apr->apr_wq);
	}
}

struct psc_thread *
acsvc_init(int thrtype, const char *name, char **av)
{
	int fds[2];
	char *p;

	if (socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, fds) == -1)
		psc_fatal("socketpair");
	switch (fork()) {
	case -1:
		psc_fatal("fork");
	case 0:  /* child */
		signal(SIGINT, SIG_IGN);
		if (av) {
			p = strrchr(av[0], '/');
			if (p)
				p++;
			else
				p = av[0];
			pfl_setprocesstitle(av, "%s [acsvc]", p);
		}
		close(fds[1]);
		acsvc_svrmain(fds[0]);
	}
	close(fds[0]);
	acsvc_fd = fds[1];

	return (pscthr_init(thrtype, acsvc_climain, 0, name));
}

__static struct access_request *
acsreq_new(int op, uid_t uid, gid_t gid)
{
	struct access_request *arq;

	arq = PSCALLOC(sizeof(*arq));
	arq->arq_op = op;
	arq->arq_uid = uid;
	arq->arq_gid = gid;
	arq->arq_id = psc_atomic32_inc_getnew(&acsvc_id);
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
	INIT_PSC_LISTENTRY(&apr->apr_lentry);
	INIT_SPINLOCK(&apr->apr_lock);
	spinlock(&apr->apr_lock);
	apr->apr_id = arq->arq_id;
	apr->apr_op = arq->arq_op;
	psc_waitq_init(&apr->apr_wq, "acsreq");

	pll_add(&acsvc_pendlist, apr);

	/* Issue request. */
	nbytes = sendmsg(acsvc_fd, &m, MSG_WAITALL);
	if (nbytes == -1)
		psc_fatal("sendmsg");
	else if (nbytes != sizeof(*arq))
		psc_fatalx("sendmsg: short I/O");
	PSCFREE(arq);

	/* Wait for request return. */
	psc_waitq_wait(&apr->apr_wq, &apr->apr_lock);	/* XXX check return */

	pll_remove(&acsvc_pendlist, apr);
	return (apr);
}

int
access_fsop(int op, uid_t uid, gid_t gid, const char *fn, ...)
{
	struct access_request *arq;
	struct access_pendreq *apr;
	va_list ap;
	int rc;

	rc = 0;
	/* Construct request. */
	arq = acsreq_new(op, uid, gid);
	if (strlcpy(arq->arq_fn, fn, PATH_MAX) >= PATH_MAX) {
		PSCFREE(arq);
		errno = ENAMETOOLONG;
		return (-1);
	}
	va_start(ap, fn);
	switch (op) {
	case ACSOP_ACCESS:
		arq->arq_data_access.mode = va_arg(ap, int);
		break;
	case ACSOP_CHMOD:
		arq->arq_data_chmod.mode = va_arg(ap, int);
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
	case ACSOP_LSTAT:
		break;
	case ACSOP_MKDIR:
		arq->arq_data_mkdir.mode = va_arg(ap, int);
		break;
	case ACSOP_MKNOD:
		arq->arq_data_mknod.mode = va_arg(ap, int);
		arq->arq_data_mknod.dev = va_arg(ap, dev_t);
		break;
	case ACSOP_OPEN:
		arq->arq_data_open.flags = va_arg(ap, int);
		if (arq->arq_data_open.flags & O_CREAT)
			arq->arq_data_open.mode = va_arg(ap, int);
		break;
	case ACSOP_READLINK:
		break;
	case ACSOP_RENAME:
		if (strlcpy(arq->arq_data_rename.to, va_arg(ap, char *),
		    PATH_MAX) >= PATH_MAX)
			rc = ENAMETOOLONG;
		break;
	case ACSOP_RMDIR:
	case ACSOP_STAT:
	case ACSOP_STATFS:
		break;
	case ACSOP_SYMLINK:
		if (strlcpy(arq->arq_data_symlink.to, va_arg(ap, char *),
		    PATH_MAX) >= PATH_MAX)
			rc = ENAMETOOLONG;
		break;
	case ACSOP_TRUNCATE:
		arq->arq_data_truncate.len = va_arg(ap, off_t);
		if (arq->arq_data_truncate.len < 0)
			rc = EINVAL;
		break;
	case ACSOP_UNLINK:
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
		case ACSOP_READLINK:
			rc = apr->apr_rep.arp_rc;
			if (rc) {
				errno = rc;
				rc = -1;
			} else {
				size_t len, rlen, tlen;
				char *buf;

				buf = va_arg(ap, char *);
				len = va_arg(ap, size_t);
				if (len > 1) {
					rlen = apr->apr_rep.arp_data_readlink.len;

					/* do copy confined to smallest bufsiz */
					tlen = MIN(rlen, len);
					memcpy(buf,
					    apr->apr_rep.arp_data_readlink.fn, tlen);

					/* if extra space for NUL, append */
					if (tlen < len && tlen > 0)
						buf[tlen] = '\0';
					/* no extra space, overwrite last char */
					else if (tlen > 0)
						buf[tlen - 1] = '\0';
					else
						buf[0] = '\0';
					rc = tlen;
				}
			}
			break;
		case ACSOP_LSTAT:
		case ACSOP_STAT:
			rc = apr->apr_rep.arp_rc;
			if (rc) {
				errno = rc;
				rc = -1;
			} else
				memcpy(va_arg(ap, struct stat *),
				    &apr->apr_rep.arp_data_stat.stb,
				    sizeof(struct stat));
			break;
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
		PSCFREE(apr); /* XXX might modify errno */
	}
	va_end(ap);
	return (rc);
}
