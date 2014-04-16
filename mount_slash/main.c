/* $Id$ */
/*
 * %PSCGPL_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2007-2014, Pittsburgh Supercomputing Center (PSC).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License contained in the file
 * `COPYING-GPL' at the top of this distribution or at
 * https://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Pittsburgh Supercomputing Center	phone: 412.268.4960  fax: 412.268.5832
 * 300 S. Craig Street			e-mail: remarks@psc.edu
 * Pittsburgh, PA 15213			web: http://www.psc.edu/
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gcrypt.h>

#include "pfl/cdefs.h"
#include "pfl/ctlsvr.h"
#include "pfl/dynarray.h"
#include "pfl/eqpollthr.h"
#include "pfl/fault.h"
#include "pfl/fs.h"
#include "pfl/fsmod.h"
#include "pfl/iostats.h"
#include "pfl/log.h"
#include "pfl/pfl.h"
#include "pfl/random.h"
#include "pfl/rpc.h"
#include "pfl/rpclog.h"
#include "pfl/rsx.h"
#include "pfl/stat.h"
#include "pfl/str.h"
#include "pfl/sys.h"
#include "pfl/thread.h"
#include "pfl/time.h"
#include "pfl/usklndthr.h"
#include "pfl/vbitmap.h"
#include "pfl/workthr.h"

#include "bmap_cli.h"
#include "cache_params.h"
#include "ctl_cli.h"
#include "fidc_cli.h"
#include "fidcache.h"
#include "mkfn.h"
#include "mount_slash.h"
#include "pathnames.h"
#include "rpc_cli.h"
#include "slashrpc.h"
#include "slerr.h"
#include "slsubsys.h"
#include "slutil.h"
#include "subsys_cli.h"

GCRY_THREAD_OPTION_PTHREAD_IMPL;

#ifdef HAVE_FUSE_BIG_WRITES
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728,big_writes"
#else
# define STD_MOUNT_OPTIONS	"allow_other,max_write=134217728"
#endif

#define MSL_FS_BLKSIZ		(256 * 1024)

#define msl_load_fcmh(pfr, inum, fp)					\
	fidc_lookup_load((inum), (fp), pscfs_getclientctx(pfr))

#define msl_peek_fcmh(pfr, inum, fp)					\
	fidc_lookup_peek((inum), (fp), pscfs_getclientctx(pfr))

#define mfh_getfid(mfh)		fcmh_2_fid((mfh)->mfh_fcmh)
#define mfh_getfg(mfh)		(mfh)->mfh_fcmh->fcmh_fg

#define MSL_FLUSH_ATTR_TIMEOUT	8

#define fcmh_reserved(f)	(fcmh_2_fid(f) == SLFID_NS ? EPERM : 0)

struct uid_mapping {
	/* these are 64-bit as limitation of hash API */
	uint64_t		um_key;
	uint64_t		um_val;
	struct psc_hashent	um_hentry;
};

struct psc_waitq		 msl_flush_attrq = PSC_WAITQ_INIT;
psc_spinlock_t			 msl_flush_attrqlock = SPINLOCK_INIT;

struct psc_listcache		 attrTimeoutQ;

sl_ios_id_t			 prefIOS = IOS_ID_ANY;
const char			*progname;
const char			*ctlsockfn = SL_PATH_MSCTLSOCK;
char				 mountpoint[PATH_MAX];
int				 use_mapfile;
int				 allow_root_uid = 1;
struct psc_dynarray		 allow_exe = DYNARRAY_INIT;

struct psc_vbitmap		 msfsthr_uniqidmap = VBITMAP_INIT_AUTO;
psc_spinlock_t			 msfsthr_uniqidmap_lock = SPINLOCK_INIT;

struct psc_poolmaster		 slc_async_req_poolmaster;
struct psc_poolmgr		*slc_async_req_pool;

struct psc_poolmaster		 slc_biorq_poolmaster;
struct psc_poolmgr		*slc_biorq_pool;

struct psc_poolmaster		 mfh_poolmaster;
struct psc_poolmgr		*mfh_pool;

struct psc_poolmaster		 mfsrq_poolmaster;
struct psc_poolmgr		*mfsrq_pool;

uint32_t			 sl_sys_upnonce;

struct psc_hashtbl		 slc_uidmap_ext;
struct psc_hashtbl		 slc_uidmap_int;

int				 slc_posix_mkgrps;

int
uidmap_ext_cred(struct srt_creds *cr)
{
	struct uid_mapping *um, q;

	if (!use_mapfile)
		return (0);

	q.um_key = cr->scr_uid;
	um = psc_hashtbl_search(&slc_uidmap_ext, NULL, NULL, &q.um_key);
	if (um == NULL)
		return (ENOENT);
	cr->scr_uid = um->um_val;
	return (0);
}

int
uidmap_ext_stat(struct srt_stat *sstb)
{
	struct uid_mapping *um, q;

	if (!use_mapfile)
		return (0);

	q.um_key = sstb->sst_uid;
	um = psc_hashtbl_search(&slc_uidmap_ext, NULL, NULL, &q.um_key);
	if (um == NULL)
		return (ENOENT);
	sstb->sst_uid = um->um_val;
	return (0);
}

int
uidmap_int_stat(struct srt_stat *sstb)
{
	struct uid_mapping *um, q;

	if (!use_mapfile)
		return (0);

	q.um_key = sstb->sst_uid;
	um = psc_hashtbl_search(&slc_uidmap_int, NULL, NULL, &q.um_key);
	if (um == NULL)
		return (ENOENT);
	sstb->sst_uid = um->um_val;
	return (0);
}

int
fcmh_checkcreds(struct fidc_membh *f, const struct pscfs_creds *pcrp,
    int accmode)
{
	int rc, locked;

	locked = FCMH_RLOCK(f);
	rc = checkcreds(&f->fcmh_sstb, pcrp, accmode);
	FCMH_URLOCK(f, locked);
	return (rc);
}

__static void
msfsthr_teardown(void *arg)
{
	struct msfs_thread *mft = arg;

	spinlock(&msfsthr_uniqidmap_lock);
	psc_vbitmap_unset(&msfsthr_uniqidmap, mft->mft_uniqid);
	psc_vbitmap_setnextpos(&msfsthr_uniqidmap, 0);
	freelock(&msfsthr_uniqidmap_lock);
}

__static void
msfsthr_ensure(void)
{
	struct msfs_thread *mft;
	struct psc_thread *thr;
	size_t id;

	thr = pscthr_get_canfail();
	if (thr == NULL) {
		spinlock(&msfsthr_uniqidmap_lock);
		if (psc_vbitmap_next(&msfsthr_uniqidmap, &id) != 1)
			psc_fatal("psc_vbitmap_next");
		freelock(&msfsthr_uniqidmap_lock);

		thr = pscthr_init(MSTHRT_FS, 0, NULL,
		    msfsthr_teardown, sizeof(*mft), "msfsthr%02zu",
		    id);
		mft = thr->pscthr_private;
		psc_multiwait_init(&mft->mft_mw, "%s",
		    thr->pscthr_name);
		mft->mft_uniqid = id;
		pscthr_setready(thr);
	}
	psc_assert(thr->pscthr_type == MSTHRT_FS);
}

/**
 * msl_create_fcmh - Create a FID cache member handle based on the
 *	statbuf provided.
 * @sstb: file stat info.
 * @setattrflags: flags to fcmh_setattrf().
 * @name: base name of file.
 * @lookupflags: fid cache lookup flags.
 * @fp: value-result fcmh.
 */
#define msl_create_fcmh(pfr, sstb, safl, fp)				\
	_fidc_lookup(PFL_CALLERINFOSS(SLSS_FCMH), &(sstb)->sst_fg,	\
	    FIDC_LOOKUP_CREATE, (sstb), (safl), (fp), pscfs_getclientctx(pfr))

void
mslfsop_access(struct pscfs_req *pfr, pscfs_inum_t inum, int accmode)
{
	struct pscfs_creds pcr;
	struct fidc_membh *c;
	int rc;

	msfsthr_ensure();

	pscfs_getcreds(pfr, &pcr);
	rc = msl_load_fcmh(pfr, inum, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	rc = 0;
	FCMH_LOCK(c);
	if (pcr.pcr_uid == 0) {
		if ((accmode & X_OK) && !S_ISDIR(c->fcmh_sstb.sst_mode) &&
		    (c->fcmh_sstb.sst_mode & _S_IXUGO) == 0)
			rc = EACCES;
	} else
		rc = fcmh_checkcreds(c, &pcr, accmode);
	FCMH_ULOCK(c);

 out:
	pscfs_reply_access(pfr, rc);
	if (c)
		fcmh_op_done(c);
}

#define msl_progallowed(r)						\
	(psc_dynarray_len(&allow_exe) == 0 || _msl_progallowed(r))

int
_msl_progallowed(struct pscfs_req *pfr)
{
	char fn[PATH_MAX], exe[PATH_MAX];
	pid_t pid, ppid;
	const char *p;
	FILE *fp;
	int n;

	ppid = pscfs_getclientctx(pfr)->pfcc_pid;
	do {
		pid = ppid;

		/* we made it to the root; disallow */
		if (pid == 0 || pid == 1)
			return (0);

		snprintf(fn, sizeof(fn), "/proc/%d/exe", pid);
		if (readlink(fn, exe, sizeof(exe)) == -1) {
			psclog_warn("unable to check access on %s", fn);
			return (0);
		}
		DYNARRAY_FOREACH(p, n, &allow_exe)
		    if (strcmp(exe, p) == 0)
			    return (1);

		snprintf(fn, sizeof(fn), "/proc/%d/stat", pid);
		fp = fopen(fn, "r");
		if (fp == NULL) {
			psclog_warn("unable to read parent PID from %s",
			    fn);
			return (0);
		}
		n = fscanf(fp, "%*d %*s %*c %d ", &ppid);
		fclose(fp);
		if (n != 1) {
			psclog_warn("unable to read parent PID from %s",
			    fn);
			return (0);
		}
	} while (pid != ppid);
	return (0);
}

void
mslfsop_create(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, int oflags, mode_t mode)
{
	struct fidc_membh *c = NULL, *p = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_create_rep *mp = NULL;
	struct srm_create_req *mq;
	struct msl_fhent *mfh = NULL;
	struct fcmh_cli_info *fci;
	struct pscfs_creds pcr;
	struct bmapc_memb *b;
	struct stat stb;
	int rc = 0;
	struct bmap_cli_info *bci;

	msfsthr_ensure();

	psc_assert(oflags & O_CREAT);
	OPSTAT_INCR(SLC_OPST_CREAT);

	if (!msl_progallowed(pfr))
		PFL_GOTOERR(out, rc = EPERM);
	if (strlen(name) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(name) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(p))
		PFL_GOTOERR(out, rc = ENOTDIR);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);
	rc = fcmh_checkcreds(p, &pcr, W_OK);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, SRMT_CREATE, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->mode = !(mode & 0777) ? (0666 & ~pscfs_getumask(pfr)) :
	    mode;
	mq->pfg.fg_fid = pinum;
	mq->pfg.fg_gen = FGEN_ANY;
	mq->prefios[0] = prefIOS;
	mq->creds.scr_uid = pcr.pcr_uid;
	mq->creds.scr_gid = slc_posix_mkgrps ?
	    p->fcmh_sstb.sst_gid : pcr.pcr_gid;
	rc = uidmap_ext_cred(&mq->creds);
	if (rc)
		PFL_GOTOERR(out, rc);
	strlcpy(mq->name, name, sizeof(mq->name));
	PFL_GETPTIMESPEC(&mq->time);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	psclog_diag("pfid="SLPRI_FID" fid="SLPRI_FID" "
	    "mode=%#o name='%s' rc=%d", pinum,
	    mp->cattr.sst_fg.fg_fid, mode, name, rc);

	uidmap_int_stat(&mp->pattr);
	fcmh_setattr(p, &mp->pattr);

	uidmap_int_stat(&mp->cattr);
	rc = msl_create_fcmh(pfr, &mp->cattr, FCMH_SETATTRF_NONE, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

#if 0
	if (oflags & O_APPEND) {
		FCMH_LOCK(c);
		c->fcmh_flags |= FCMH_CLI_APPENDWR;
		FCMH_ULOCK(c);
	}

	if (oflags & O_SYNC) {
		/* XXX do we need to do anything special for this? */
	}
	if (oflags & O_NONBLOCK) {
		/* XXX do we need to do anything special for this? */
	}
#endif

	mfh = msl_fhent_new(pfr, c);
	mfh->mfh_oflags = oflags;
	PFL_GETTIMESPEC(&mfh->mfh_open_time);
	memcpy(&mfh->mfh_open_atime, &c->fcmh_sstb.sst_atime,
	    sizeof(mfh->mfh_open_atime));

	FCMH_LOCK(c);
	sl_internalize_stat(&c->fcmh_sstb, &stb);

	if (mp->rc2)
		PFL_GOTOERR(out, rc = mp->rc2);

	fci = fcmh_2_fci(c);
	fci->fci_inode.reptbl[0].bs_id = mp->sbd.sbd_ios;
	fci->fci_inode.nrepls = 1;
	c->fcmh_flags |= FCMH_CLI_HAVEINODE;
	// XXX bug fci->fci_inode.flags inherited?
	// XXX bug fci->fci_inode.newreplpol inherited?
	FCMH_ULOCK(c);

	/* XXX this load should be async so we can reply quickly */
	mp->rc2 = bmap_getf(c, 0, SL_WRITE, BMAPGETF_LOAD |
	    BMAPGETF_NORETRIEVE, &b);
	if (mp->rc2)
		PFL_GOTOERR(out, rc = mp->rc2);

	msl_bmap_reap_init(b, &mp->sbd);

	DEBUG_BMAP(PLL_DIAG, b, "ios(%s) sbd_seq=%"PRId64,
	    libsl_ios2name(mp->sbd.sbd_ios), mp->sbd.sbd_seq);

	bci = bmap_2_bci(b);
	SL_REPL_SET_BMAP_IOS_STAT(bci->bci_repls, 0, BREPLST_VALID);

	bmap_op_done(b);

 out:
	psclog_info("create: pfid="SLPRI_FID" name='%s' mode=%#x flag=%#o rc=%d",
	    pinum, name, mode, oflags, rc);

	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);

	pscfs_reply_create(pfr, mp ? mp->cattr.sst_fid : 0,
	    mp ? mp->cattr.sst_gen : 0, pscfs_entry_timeout, &stb,
	    pscfs_attr_timeout, mfh, PSCFS_CREATEF_DIO, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
	OPSTAT_INCR(SLC_OPST_CREAT_DONE);
}

__static int
msl_open(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags,
    struct msl_fhent **mfhp, int *rflags)
{
	struct fidc_membh *c = NULL;
	struct pscfs_creds pcr;
	int rc = 0;

	msfsthr_ensure();

	pscfs_getcreds(pfr, &pcr);

	*mfhp = NULL;

	if (!msl_progallowed(pfr))
		PFL_GOTOERR(out, rc = EPERM);

	rc = msl_load_fcmh(pfr, inum, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	if ((oflags & O_ACCMODE) != O_WRONLY) {
		rc = fcmh_checkcreds(c, &pcr, R_OK);
		if (rc)
			PFL_GOTOERR(out, rc);
	}
	if (oflags & (O_WRONLY | O_RDWR)) {
		rc = fcmh_checkcreds(c, &pcr, W_OK);
		if (rc)
			PFL_GOTOERR(out, rc);
	}

	/* Perform rudimentary directory sanity checks. */
	if (fcmh_isdir(c)) {
		/* pscfs shouldn't ever pass us WR with a dir */
		psc_assert((oflags & (O_WRONLY | O_RDWR)) == 0);
		if (!(oflags & O_DIRECTORY))
			PFL_GOTOERR(out, rc = EISDIR);
	} else {
		if (oflags & O_DIRECTORY)
			PFL_GOTOERR(out, rc = ENOTDIR);
	}

	*mfhp = msl_fhent_new(pfr, c);
	(*mfhp)->mfh_oflags = oflags;
	PFL_GETTIMESPEC(&(*mfhp)->mfh_open_time);
	memcpy(&(*mfhp)->mfh_open_atime, &c->fcmh_sstb.sst_atime,
	    sizeof((*mfhp)->mfh_open_atime));

	if (oflags & O_DIRECTORY)
		*rflags |= PSCFS_OPENF_KEEPCACHE;

	/*
	 * PSCFS direct_io does not work with mmap(MAP_SHARED), which is
	 * what the kernel uses under the hood when running executables,
	 * so disable it for this case.
	 */
	if ((c->fcmh_sstb.sst_mode &
	    (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
		*rflags |= PSCFS_OPENF_DIO;

	if (oflags & O_TRUNC) {
		/*
		 * XXX write me for pscfs backends that do not separate
		 * SETATTR st_mode=0
		 */
	}
	if (oflags & O_SYNC) {
		/* XXX write me */
	}
	if (oflags & O_NONBLOCK) {
		/* XXX write me */
	}

 out:
	if (c) {
		DEBUG_FCMH(PLL_DIAG, c, "new mfh=%p dir=%s rc=%d oflags=%#o",
		    *mfhp, (oflags & O_DIRECTORY) ? "yes" : "no", rc, oflags);
		fcmh_op_done(c);
	}
	return (rc);
}

void
mslfsop_open(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	struct msl_fhent *mfh;
	int rflags, rc;

	rflags = 0;
	rc = msl_open(pfr, inum, oflags, &mfh, &rflags);
	pscfs_reply_open(pfr, mfh, rflags, rc);
}

void
mslfsop_opendir(struct pscfs_req *pfr, pscfs_inum_t inum, int oflags)
{
	struct msl_fhent *mfh;
	int rflags, rc;

	rflags = 0;
	rc = msl_open(pfr, inum, oflags | O_DIRECTORY, &mfh, &rflags);
	pscfs_reply_opendir(pfr, mfh, rflags, rc);
}

int
msl_stat(struct fidc_membh *f, void *arg)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscfs_clientctx *pfcc = arg;
	struct pscrpc_request *rq = NULL;
	struct srm_getattr_req *mq;
	struct srm_getattr_rep *mp;
	struct fcmh_cli_info *fci;
	struct timeval now;
	int rc = 0;

	/*
	 * Special case to handle accesses to
	 * /$mountpoint/.slfidns/<fid>
	 */
	if (fcmh_2_fid(f) == SLFID_NS) {
		struct srt_stat sstb;

		memset(&sstb, 0, sizeof(sstb));
		sstb.sst_fid = SLFID_NS;
		sstb.sst_mode = S_IFDIR | 0111;
		sstb.sst_nlink = 2;
		sstb.sst_size = 2;
		sstb.sst_blksize = MSL_FS_BLKSIZ;
		sstb.sst_blocks = 4;
		fcmh_setattrf(f, &sstb, 0);
		return (0);
	}

	fci = fcmh_2_fci(f);

	FCMH_LOCK(f);
	fcmh_wait_locked(f, f->fcmh_flags & FCMH_GETTING_ATTRS);

	if (f->fcmh_flags & FCMH_HAVE_ATTRS) {
		PFL_GETTIMEVAL(&now);
		if (timercmp(&now, &fci->fci_age, <)) {
			DEBUG_FCMH(PLL_INFO, f,
			    "attrs retrieved from local cache");
			FCMH_ULOCK(f);
			return (0);
		}
	}

	/* Attrs have expired or do not exist. */
	f->fcmh_flags |= FCMH_GETTING_ATTRS;
	FCMH_ULOCK(f);

	do {
		MSL_RMC_NEWREQ_PFCC(pfcc, f, csvc, SRMT_GETATTR, rq, mq,
		    mp, rc);
		if (rc)
			break;

		mq->fg = f->fcmh_fg;
		mq->iosid = prefIOS;

		rc = SL_RSX_WAITREP(csvc, rq, mp);
	} while (rc && slc_rmc_retry_pfcc(pfcc, &rc));

	if (rc == 0) {
		rc = mp->rc;
		uidmap_int_stat(&mp->attr);
	}

	FCMH_LOCK(f);
	if (!rc && fcmh_2_fid(f) != mp->attr.sst_fid)
		rc = EBADF;
	if (!rc) {
		fcmh_setattrf(f, &mp->attr,
		    FCMH_SETATTRF_SAVELOCAL | FCMH_SETATTRF_HAVELOCK);
		fci->fci_xattrsize = mp->xattrsize;
	}

	f->fcmh_flags &= ~FCMH_GETTING_ATTRS;
	fcmh_wake_locked(f);

	if (rq)
		pscrpc_req_finished(rq);

	DEBUG_FCMH(PLL_DEBUG, f, "attrs retrieved via rpc rc=%d", rc);

	FCMH_ULOCK(f);
	if (csvc)
		sl_csvc_decref(csvc);
	return (rc);
}

void
mslfsop_getattr(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	struct pscfs_creds pcr;
	struct fidc_membh *f;
	struct stat stb;
	int rc;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_GETATTR);
	pscfs_getcreds(pfr, &pcr);
	/*
	 * Lookup and possibly create a new fidcache handle for inum.
	 * If the fid does not exist in the cache then a placeholder
	 * will be allocated.  msl_stat() will detect incomplete attrs
	 * via FCMH_GETTING_ATTRS flag and RPC for them.
	 */
	rc = msl_load_fcmh(pfr, inum, &f);
	if (rc)
		PFL_GOTOERR(out, rc);

	rc = msl_stat(f, pfr);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(f))
		f->fcmh_sstb.sst_blksize = MSL_FS_BLKSIZ;

	FCMH_LOCK(f);
	sl_internalize_stat(&f->fcmh_sstb, &stb);

 out:
	if (f)
		fcmh_op_done(f);
	pscfs_reply_getattr(pfr, &stb, pscfs_attr_timeout, rc);
	DEBUG_STATBUF(rc ? PLL_INFO : PLL_DIAG, &stb, "getattr rc=%d",
	    rc);
}

void
mslfsop_link(struct pscfs_req *pfr, pscfs_inum_t c_inum,
    pscfs_inum_t p_inum, const char *newname)
{
	struct fidc_membh *p = NULL, *c = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_link_rep *mp = NULL;
	struct srm_link_req *mq;
	struct pscfs_creds pcr;
	struct stat stb;
	int rc = 0;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_LINK);
	if (strlen(newname) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(newname) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	pscfs_getcreds(pfr, &pcr);

	/* Check the parent inode. */
	rc = msl_load_fcmh(pfr, p_inum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(p))
		PFL_GOTOERR(out, rc = ENOTDIR);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

	rc = fcmh_checkcreds(p, &pcr, W_OK);
	if (rc)
		PFL_GOTOERR(out, rc);

	/* Check the child inode. */
	rc = msl_load_fcmh(pfr, c_inum, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (fcmh_isdir(c))
		PFL_GOTOERR(out, rc = EISDIR);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, SRMT_LINK, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->pfg = p->fcmh_fg;
	mq->fg = c->fcmh_fg;
	strlcpy(mq->name, newname, sizeof(mq->name));

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	uidmap_int_stat(&mp->pattr);
	fcmh_setattr(p, &mp->pattr);

	FCMH_LOCK(c);
	uidmap_int_stat(&mp->cattr);
	fcmh_setattrf(c, &mp->cattr, FCMH_SETATTRF_SAVELOCAL |
	    FCMH_SETATTRF_HAVELOCK);
	sl_internalize_stat(&c->fcmh_sstb, &stb);

 out:
	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);

	pscfs_reply_link(pfr, mp ? mp->cattr.sst_fid : 0,
	    mp ? mp->cattr.sst_gen : 0, pscfs_entry_timeout, &stb,
	    pscfs_attr_timeout, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

void
mslfsop_mkdir(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, mode_t mode)
{
	struct fidc_membh *c = NULL, *p = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_mkdir_rep *mp = NULL;
	struct srm_mkdir_req *mq;
	struct pscfs_creds pcr;
	struct stat stb;
	int rc;

	msfsthr_ensure();
	OPSTAT_INCR(SLC_OPST_MKDIR);

	if (strlen(name) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(name) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(p))
		PFL_GOTOERR(out, rc = ENOTDIR);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, SRMT_MKDIR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->pfg.fg_fid = pinum;
	mq->pfg.fg_gen = FGEN_ANY;
	mq->sstb.sst_uid = pcr.pcr_uid;
	mq->sstb.sst_gid = slc_posix_mkgrps ?
	    p->fcmh_sstb.sst_gid : pcr.pcr_gid;
	rc = uidmap_ext_stat(&mq->sstb);
	if (rc)
		PFL_GOTOERR(out, rc);
	mq->sstb.sst_mode = mode;
	mq->to_set = PSCFS_SETATTRF_MODE;
	strlcpy(mq->name, name, sizeof(mq->name));

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	uidmap_int_stat(&mp->pattr);
	fcmh_setattr(p, &mp->pattr);

	uidmap_int_stat(&mp->cattr);
	rc = msl_create_fcmh(pfr, &mp->cattr, FCMH_SETATTRF_NONE, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	sl_internalize_stat(&mp->cattr, &stb);

 out:
	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);

	psclog_diag("pfid="SLPRI_FID" mode=%#o name='%s' rc=%d",
	    pinum, mode, name, rc);

	pscfs_reply_mkdir(pfr, mp ? mp->cattr.sst_fid : 0,
	    mp ? mp->cattr.sst_gen : 0, pscfs_entry_timeout, &stb,
	    pscfs_attr_timeout, -rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

__static int
msl_lookuprpc(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, struct slash_fidgen *fgp, struct srt_stat *sstb,
    struct fidc_membh **fp)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct fidc_membh *m = NULL;
	struct srm_lookup_req *mq;
	struct srm_lookup_rep *mp;
	int rc;

	if (strlen(name) == 0)
		return (ENOENT);
	if (strlen(name) > SL_NAME_MAX)
		return (ENAMETOOLONG);

 retry:
	MSL_RMC_NEWREQ(pfr, NULL, csvc, SRMT_LOOKUP, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->pfg.fg_fid = pinum;
	mq->pfg.fg_gen = FGEN_ANY;
	strlcpy(mq->name, name, sizeof(mq->name));

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	/*
	 * Add the inode to the cache first, otherwise pscfs may come to
	 * us with another request for the inode since it won't yet be
	 * visible in the cache.
	 */
	uidmap_int_stat(&mp->attr);
	rc = msl_create_fcmh(pfr, &mp->attr, FCMH_SETATTRF_SAVELOCAL,
	    &m);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (fgp)
		*fgp = mp->attr.sst_fg;

	if (sstb) {
		FCMH_LOCK(m);
		*sstb = m->fcmh_sstb;
		FCMH_ULOCK(m);
	}

	// XXX add to dircache

 out:
	psclog_diag("pfid="SLPRI_FID" name='%s' cfid="SLPRI_FID" rc=%d",
	    pinum, name, m ? m->fcmh_sstb.sst_fid : FID_ANY, rc);
	if (rc == 0 && fp)
		*fp = m;
	else if (m)
		fcmh_op_done(m);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
	return (rc);
}

int
msl_readdir_issue(struct pscfs_clientctx *, struct fidc_membh *, off_t,
    size_t, int);

int
slc_wk_issue_readdir(void *p)
{
	struct slc_wkdata_readdir *wk = p;

	msl_readdir_issue(NULL, wk->d, wk->off, wk->size, 0);
	FCMH_LOCK(wk->d);
	wk->pg->dcp_refcnt--;
	fcmh_op_done_type(wk->d, FCMH_OPCNT_WORKER);
	return (0);
}

/**
 * Register a 'miss' in the FID namespace lookup cache.
 * If we reach a threshold, we issue an asynchronous READDIR in hopes
 * that we will hit subsequent requests.
 */
void
lookup_cache_tally_miss(struct fidc_membh *p, off_t off)
{
	struct fcmh_cli_info *pi = fcmh_2_fci(p);
	struct slc_wkdata_readdir *wk = NULL;
	struct timeval ts, delta;
	struct dircache_page *np;
	int ra = 0;

	FCMH_LOCK(p);
	PFL_GETTIMEVAL(&ts);
	timersub(&ts, &pi->fcid_lookup_age, &delta);
	if (delta.tv_sec > 1) {
		pi->fcid_lookup_age = ts;
		pi->fcid_lookup_misses = pi->fcid_lookup_misses >>
		    delta.tv_sec;
	}
	pi->fcid_lookup_misses += DIR_LOOKUP_MISSES_INCR;
	if (pi->fcid_lookup_misses >= DIR_LOOKUP_MISSES_THRES)
		ra = 1;
	FCMH_ULOCK(p);

	if (!ra)
		return;

	np = dircache_new_page(p, off, 0);
	if (np == NULL)
		return;

	wk = pfl_workq_getitem(slc_wk_issue_readdir,
	    struct slc_wkdata_readdir);
	fcmh_op_start_type(p, FCMH_OPCNT_WORKER);
	wk->d = p;
	wk->pg = np;
	wk->off = off;
	wk->size = 32 * 1024;
	pfl_workq_putitem(wk);
}

__static int
msl_lookup_fidcache(struct pscfs_req *pfr,
    const struct pscfs_creds *pcrp, pscfs_inum_t pinum,
    const char *name, struct slash_fidgen *fgp, struct srt_stat *sstb,
    struct fidc_membh **fp)
{
	struct fidc_membh *p, *c = NULL;
	slfid_t cfid = FID_ANY;
	int remote = 0, rc;
	off_t nextoff;

	if (fp)
		*fp = NULL;

#define MSL_FIDNS_RPATH	".slfidns"
	if (pinum == SLFID_ROOT && strcmp(name, MSL_FIDNS_RPATH) == 0) {
		struct fidc_membh f;

		memset(&f, 0, sizeof(f));
		INIT_SPINLOCK(&f.fcmh_lock);
		fcmh_2_fid(&f) = SLFID_NS;
		msl_stat(&f, NULL);
		if (fgp) {
			fgp->fg_fid = SLFID_NS;
			fgp->fg_gen = 0;
		}
		if (sstb)
			*sstb = f.fcmh_sstb;
		return (0);
	}
	if (pinum == SLFID_NS) {
		slfid_t fid;
		char *endp;

		fid = strtoll(name, &endp, 16);
		if (endp == name || *endp != '\0')
			return (ENOENT);
		rc = msl_load_fcmh(pfr, fid, &c);
		if (rc)
			return (-rc);
		if (fgp)
			*fgp = c->fcmh_fg;
		if (sstb) {
			FCMH_LOCK(c);
			*sstb = c->fcmh_sstb;
		}
		PFL_GOTOERR(out, rc);
	}

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	FCMH_LOCK(p);
	rc = fcmh_checkcreds(p, pcrp, X_OK);
	if (rc) {
		fcmh_op_done(p);
		PFL_GOTOERR(out, rc);
	}

	cfid = dircache_lookup(p, name, &nextoff);
	FCMH_ULOCK(p);
	if (cfid == FID_ANY || fidc_lookup_fid(cfid, &c)) {
		OPSTAT_INCR(SLC_OPST_DIRCACHE_LOOKUP_MISS);
		lookup_cache_tally_miss(p, nextoff);
		remote = 1;
	}

	fcmh_op_done(p);

	if (remote)
		return (msl_lookuprpc(pfr, pinum, name, fgp, sstb, fp));

	/*
	 * We should do a lookup based on name here because a rename
	 * does not change the file ID and we would get a success in a
	 * stat RPC.  Note the call is looking based on a name here, not
	 * based on FID.
	 */
	rc = msl_stat(c, pfr);
	if (!rc) {
		FCMH_LOCK(c);
		if (fgp)
			*fgp = c->fcmh_fg;
		if (sstb)
			*sstb = c->fcmh_sstb;
		FCMH_ULOCK(c);
	}

 out:
	if (rc == 0 && fp)
		*fp = c;
	else if (c)
		fcmh_op_done(c);

	psclog_info("look for file: %s under inode: "SLPRI_FID" rc=%d",
	    name, pinum, rc);

	return (rc);
}

__static int
msl_delete(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, int isfile)
{
	struct fidc_membh *c = NULL, *p = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_unlink_req *mq;
	struct srm_unlink_rep *mp;
	struct pscfs_creds pcr;
	int rc;

	msfsthr_ensure();

	if (strlen(name) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(name) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);

	FCMH_LOCK(p);
	if ((p->fcmh_sstb.sst_mode & S_ISVTX) && pcr.pcr_uid) {
		if (p->fcmh_sstb.sst_uid != pcr.pcr_uid) {
			struct srt_stat sstb;

			FCMH_ULOCK(p);

			rc = msl_lookup_fidcache(pfr, &pcr, pinum, name,
			    NULL, &sstb, NULL);
			if (rc)
				PFL_GOTOERR(out, rc);

			if (sstb.sst_uid != pcr.pcr_uid)
				rc = EPERM;
		} else
			FCMH_ULOCK(p);
	} else {
		rc = fcmh_checkcreds(p, &pcr, W_OK);
		FCMH_ULOCK(p);
	}
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, isfile ? SRMT_UNLINK : SRMT_RMDIR,
	    rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->pfid = pinum;
	strlcpy(mq->name, name, sizeof(mq->name));

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc)
		PFL_GOTOERR(out, rc);
	if (rc == 0)
		rc = mp->rc;

	if (!rc) {
		FCMH_LOCK(p);
		uidmap_int_stat(&mp->pattr);
		fcmh_setattr_locked(p, &mp->pattr);
		FCMH_ULOCK(p);
	}

	if (!rc) {
		int tmprc;

		tmprc = msl_peek_fcmh(pfr, mp->cattr.sst_fid, &c);
		if (!tmprc) {
			if (mp->valid) {
				uidmap_int_stat(&mp->cattr);
				fcmh_setattrf(c, &mp->cattr,
				    FCMH_SETATTRF_SAVELOCAL);
			} else {
				FCMH_LOCK(c);
				c->fcmh_flags |= FCMH_DELETED;
				OPSTAT_INCR(SLC_OPST_DELETE_MARKED);
			}
		} else
			OPSTAT_INCR(SLC_OPST_DELETE_SKIPPED);
	}

	psclog_info("delete: fid="SLPRI_FG" valid = %d name='%s' isfile=%d rc=%d",
	    SLPRI_FG_ARGS(&mp->cattr.sst_fg), mp->valid, name, isfile, rc);

 out:

	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
	return (rc);
}

void
mslfsop_unlink(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	OPSTAT_INCR(SLC_OPST_UNLINK);
	pscfs_reply_unlink(pfr, msl_delete(pfr, pinum, name, 1));
	OPSTAT_INCR(SLC_OPST_UNLINK_DONE);
}

void
mslfsop_rmdir(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	OPSTAT_INCR(SLC_OPST_RMDIR);
	pscfs_reply_unlink(pfr, msl_delete(pfr, pinum, name, 0));
	OPSTAT_INCR(SLC_OPST_RMDIR_DONE);
}

void
mslfsop_mknod(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name, mode_t mode, dev_t rdev)
{
	struct fidc_membh *p = NULL, *c = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_mknod_rep *mp = NULL;
	struct srm_mknod_req *mq = NULL;
	struct pscfs_creds pcr;
	struct stat stb;
	int rc;

	msfsthr_ensure();
	OPSTAT_INCR(SLC_OPST_MKNOD);

	if (!S_ISFIFO(mode))
		PFL_GOTOERR(out, rc = ENOTSUP);
	if (strlen(name) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(name) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(p))
		PFL_GOTOERR(out, rc = ENOTDIR);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);

	rc = fcmh_checkcreds(p, &pcr, W_OK);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, SRMT_MKNOD, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->creds.scr_uid = pcr.pcr_uid;
	mq->creds.scr_gid = slc_posix_mkgrps ?
	    p->fcmh_sstb.sst_gid : pcr.pcr_gid;
	rc = uidmap_ext_cred(&mq->creds);
	if (rc)
		PFL_GOTOERR(out, rc);
	mq->pfg.fg_fid = pinum;
	mq->pfg.fg_gen = FGEN_ANY;
	mq->mode = mode;
	mq->rdev = rdev;
	strlcpy(mq->name, name, sizeof(mq->name));

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	psclog_info("pfid="SLPRI_FID" mode=%#o name='%s' rc=%d mp->rc=%d",
	    mq->pfg.fg_fid, mq->mode, mq->name, rc, mp->rc);

	uidmap_int_stat(&mp->pattr);
	fcmh_setattr(p, &mp->pattr);

	uidmap_int_stat(&mp->cattr);
	rc = msl_create_fcmh(pfr, &mp->cattr, FCMH_SETATTRF_NONE, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	sl_internalize_stat(&mp->cattr, &stb);

 out:
	pscfs_reply_mknod(pfr, mp ? mp->cattr.sst_fid : 0,
	    mp ? mp->cattr.sst_gen : 0, pscfs_entry_timeout, &stb,
	    pscfs_attr_timeout, rc);
	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

void
msl_readdir_error(struct fidc_membh *d, struct dircache_page *p, int rc)
{
	FCMH_LOCK(d);
	p->dcp_refcnt--;
	DBGPR_DIRCACHEPG(PLL_DEBUG, p, "error rc=%d", rc);
	if (p->dcp_flags & DIRCACHEPGF_LOADING) {
		p->dcp_flags &= ~DIRCACHEPGF_LOADING;
		p->dcp_flags |= DIRCACHEPGF_LOADED;
		p->dcp_rc = rc;
		PFL_GETPTIMESPEC(&p->dcp_tm);
		fcmh_wake_locked(d);
	}
	FCMH_ULOCK(d);
}

void
msl_readdir_finish(struct fidc_membh *d, struct dircache_page *p,
    int eof, int nents, int size, struct iovec *iov)
{
	struct srt_readdir_ent *e;
	struct fidc_membh *f;
	int i;

	for (i = 0, e = iov[1].iov_base; i < nents; i++, e++) {
		if (e->sstb.sst_fid == FID_ANY ||
		    e->sstb.sst_fid == 0) {
			DEBUG_SSTB(PLL_WARN, &e->sstb,
			    "invalid readdir prefetch FID "
			    "parent@%p="SLPRI_FID, d, fcmh_2_fid(d));
			continue;
		}

		DEBUG_SSTB(PLL_DEBUG, &e->sstb, "prefetched");

		uidmap_int_stat(&e->sstb);

		fidc_lookup(&e->sstb.sst_fg, FIDC_LOOKUP_CREATE,
		    &e->sstb, FCMH_SETATTRF_SAVELOCAL, &f);

		if (f) {
			FCMH_LOCK(f);
			fcmh_2_fci(f)->fci_xattrsize =
			    e->xattrsize;
			fcmh_op_done(f);
		}
	}
	DBGPR_DIRCACHEPG(PLL_DEBUG, p, "registering");
	dircache_reg_ents(d, p, nents, iov[0].iov_base, size, eof);
}

int
msl_readdir_cb(struct pscrpc_request *rq, struct pscrpc_async_args *av)
{
	struct slashrpc_cservice *csvc = av->pointer_arg[MSL_READDIR_CBARG_CSVC];
	struct dircache_page *p = av->pointer_arg[MSL_READDIR_CBARG_PAGE];
	struct fidc_membh *d = av->pointer_arg[MSL_READDIR_CBARG_FCMH];
	struct srm_readdir_req *mq;
	struct srm_readdir_rep *mp;
	int rc;

	SL_GET_RQ_STATUS_TYPE(csvc, rq, struct srm_readdir_rep, rc);

	if (rc) {
		DEBUG_REQ(PLL_ERROR, rq, "rc=%d", rc);
		msl_readdir_error(d, p, rc);
	} else {
		slrpc_rep_in(csvc, rq);
		mq = pscrpc_msg_buf(rq->rq_reqmsg, 0, sizeof(*mq));
		mp = pscrpc_msg_buf(rq->rq_repmsg, 0, sizeof(*mp));
		if (SRM_READDIR_BUFSZ(mp->size, mp->num) <=
		    sizeof(mp->ents)) {
			struct iovec iov[2];

			iov[0].iov_base = PSCALLOC(mp->size);
			memcpy(iov[0].iov_base, mp->ents, mp->size);
			iov[1].iov_base = mp->ents + mp->size;
			msl_readdir_finish(d, p, mp->eof, mp->num,
			    mp->size, iov);
		} else {
			FCMH_LOCK(d);
			p->dcp_refcnt--;
			DBGPR_DIRCACHEPG(PLL_DEBUG, p, "decr");
		}
	}
	fcmh_op_done_type(d, FCMH_OPCNT_READDIR);
	sl_csvc_decref(csvc);
	return (0);
}

int
msl_readdir_issue(struct pscfs_clientctx *pfcc, struct fidc_membh *d,
    off_t off, size_t size, int wait)
{
	struct slashrpc_cservice *csvc = NULL;
	struct srm_readdir_req *mq = NULL;
	struct srm_readdir_rep *mp = NULL;
	struct pscrpc_request *rq = NULL;
	struct dircache_page *p;
	int rc;

	p = dircache_new_page(d, off, wait);
	if (p == NULL)
		return (-ESRCH);

	fcmh_op_start_type(d, FCMH_OPCNT_READDIR);

	MSL_RMC_NEWREQ_PFCC(pfcc, d, csvc, SRMT_READDIR, rq, mq, mp,
	    rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg = d->fcmh_fg;
	mq->size = size;
	mq->offset = off;

	rq->rq_interpret_reply = msl_readdir_cb;
	rq->rq_async_args.pointer_arg[MSL_READDIR_CBARG_CSVC] = csvc;
	rq->rq_async_args.pointer_arg[MSL_READDIR_CBARG_FCMH] = d;
	rq->rq_async_args.pointer_arg[MSL_READDIR_CBARG_PAGE] = p;
	DBGPR_DIRCACHEPG(PLL_DEBUG, p, "issuing");
	rc = SL_NBRQSET_ADD(csvc, rq);
	if (!rc)
		return (0);

	pscrpc_req_finished(rq);
	sl_csvc_decref(csvc);

 out:
	FCMH_LOCK(d);
	dircache_free_page(d, p);
	fcmh_op_done_type(d, FCMH_OPCNT_READDIR);
	return (rc);
}

void
mslfsop_readdir(struct pscfs_req *pfr, size_t size, off_t off,
    void *data)
{
	int hit = 1, j, nd, issue, rc;
	struct dircache_page *p, *np;
	struct pscfs_clientctx *pfcc;
	struct msl_fhent *mfh = data;
	struct dircache_expire dexp;
	struct fcmh_cli_info *fci;
	struct pscfs_dirent *pfd;
	struct pscfs_creds pcr;
	struct fidc_membh *d;

	OPSTAT_INCR(SLC_OPST_READDIR);

	msfsthr_ensure();

	if (off < 0 || size > 1024 * 1024)
		PFL_GOTOERR(out, rc = EINVAL);

	pfcc = pscfs_getclientctx(pfr);

	d = mfh->mfh_fcmh;
	psc_assert(d);

	if (!fcmh_isdir(d)) {
		DEBUG_FCMH(PLL_ERROR, d,
		    "inconsistency: readdir on a non-dir");
		PFL_GOTOERR(out, rc = ENOTDIR);
	}
	rc = fcmh_reserved(d);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);

	rc = fcmh_checkcreds(d, &pcr, R_OK);
	if (rc)
		PFL_GOTOERR(out, rc);

	FCMH_LOCK(d);
	fci = fcmh_2_fci(d);

 restart:
	DIRCACHEPG_INITEXP(&dexp);

	issue = 1;
	PLL_FOREACH_SAFE(p, np, &fci->fci_dc_pages) {
		if (DIRCACHEPG_EXPIRED(d, p, &dexp)) {
			dircache_free_page(d, p);
			continue;
		}

		if (p->dcp_flags & DIRCACHEPGF_LOADING) {
			/// XXX need to wake up if csvc fails
			OPSTAT_INCR(SLC_OPST_DIRCACHE_WAIT);
			fcmh_wait_nocond_locked(d);
			goto restart;
		}

		/* We found the last page; return EOF. */
		if (off == p->dcp_nextoff &&
		    p->dcp_flags & DIRCACHEPGF_EOF) {
			FCMH_ULOCK(d);
			OPSTAT_INCR(SLC_OPST_DIRCACHE_HIT_EOF);
			pscfs_reply_readdir(pfr, NULL, 0, rc);
			return;
		}

		if (dircache_hasoff(p, off)) {
			if (p->dcp_rc) {
				rc = p->dcp_rc;
				dircache_free_page(d, p);
				if (!slc_rmc_retry(pfr, &rc)) {
					FCMH_ULOCK(d);
					pscfs_reply_readdir(pfr, NULL,
					    0, rc);
					return;
				}
				break;
			} else {
				off_t poff, thisoff = p->dcp_off;
				size_t len, tlen;

				/* find starting entry */
				poff = 0;
				nd = psc_dynarray_len(p->dcp_dents_name);
				for (j = 0, pfd = p->dcp_base;
				    j < nd; j++) {
					if (off == thisoff)
						break;
					poff += PFL_DIRENT_SIZE(
					    pfd->pfd_namelen);
					thisoff = pfd->pfd_off;
					pfd = PSC_AGP(p->dcp_base,
					    poff);
				}

				/* determine size */
				for (len = 0; j < nd; j++)  {
					tlen = PFL_DIRENT_SIZE(
					    pfd->pfd_namelen);
					if (tlen + len > size)
						break;
					len += tlen;
					pfd = PSC_AGP(p->dcp_base,
					    poff + len);
				}

				// XXX I/O: remove from lock
				pscfs_reply_readdir(pfr,
				    p->dcp_base + poff, len, 0);
				p->dcp_flags |= DIRCACHEPGF_READ;
				if (hit)
					OPSTAT_INCR(SLC_OPST_DIRCACHE_HIT);

				issue = 0;
				break;
			}
		}
	}
	FCMH_ULOCK(d);

	if (issue) {
		hit = 0;
		rc = msl_readdir_issue(pfcc, d, off, size, 1);
		OPSTAT_INCR(SLC_OPST_DIRCACHE_ISSUE);
		if (rc && !slc_rmc_retry(pfr, &rc)) {
			pscfs_reply_readdir(pfr, NULL, 0, rc);
			return;
		}
		FCMH_LOCK(d);
		goto restart;
	}

	if (0)
 out:
		pscfs_reply_readdir(pfr, NULL, 0, rc);
}

void
mslfsop_lookup(struct pscfs_req *pfr, pscfs_inum_t pinum,
    const char *name)
{
	struct slash_fidgen fg;
	struct pscfs_creds pcr;
	struct srt_stat sstb;
	struct stat stb;
	int rc;

	msfsthr_ensure();
	memset(&sstb, 0, sizeof(sstb));

	pscfs_getcreds(pfr, &pcr);
	rc = msl_lookup_fidcache(pfr, &pcr, pinum, name, &fg, &sstb,
	    NULL);
	if (rc == ENOENT)
		sstb.sst_fid = 0;
	sl_internalize_stat(&sstb, &stb);
	if (!S_ISDIR(stb.st_mode))
		stb.st_blksize = MSL_FS_BLKSIZ;
	pscfs_reply_lookup(pfr, sstb.sst_fid, sstb.sst_gen,
	    pscfs_entry_timeout, &stb, pscfs_attr_timeout, rc);
}

void
mslfsop_readlink(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_readlink_req *mq;
	struct srm_readlink_rep *mp;
	struct fidc_membh *c = NULL;
	struct pscfs_creds pcr;
	struct iovec iov;
	char buf[SL_PATH_MAX];
	int rc;

	msfsthr_ensure();

	rc = msl_load_fcmh(pfr, inum, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	pscfs_getcreds(pfr, &pcr);

	rc = fcmh_checkcreds(c, &pcr, R_OK);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, c, csvc, SRMT_READLINK, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg = c->fcmh_fg;

	memset(buf, 0, sizeof(buf));

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	slrpc_bulkclient(rq, BULK_PUT_SINK, SRMC_BULK_PORTAL, &iov, 1);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;

	buf[sizeof(buf) - 1] = '\0';

 out:
	if (c)
		fcmh_op_done(c);

	pscfs_reply_readlink(pfr, buf, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

/**
 * msl_flush_int_locked - Perform main data flush operation.
 * @mfh: handle corresponding to process file descriptor.
 * Note that this function is called (at least) once for each open.
 */
__static int
msl_flush_int_locked(struct msl_fhent *mfh, int wait)
{
	struct bmpc_ioreq *r;

	if (mfh->mfh_flush_rc) {
		int rc;

		rc = mfh->mfh_flush_rc;
		mfh->mfh_flush_rc = 0;
		return (rc);
	}

	if (pll_empty(&mfh->mfh_biorqs)) {
		mfh->mfh_flush_rc = 0;
		return (0);
	}

	PLL_FOREACH(r, &mfh->mfh_biorqs) {
		BIORQ_LOCK(r);
		if (!r->biorq_ref)
			r->biorq_flags |= BIORQ_FORCE_EXPIRE;
		DEBUG_BIORQ(PLL_DIAG, r, "force expire");
		BIORQ_ULOCK(r);
	}
	bmap_flushq_wake(BMAPFLSH_EXPIRE);

	if (wait)
		while (!pll_empty(&mfh->mfh_biorqs)) {
			psc_waitq_wait(&msl_fhent_flush_waitq,
			    &mfh->mfh_lock);
			spinlock(&mfh->mfh_lock);
		}

	return (0);
}

void
mslfsop_flush(struct pscfs_req *pfr, void *data)
{
	struct msl_fhent *mfh = data;
	int rc;

	OPSTAT_INCR(SLC_OPST_FLUSH);
	DEBUG_FCMH(PLL_DIAG, mfh->mfh_fcmh, "flushing (mfh=%p)", mfh);

	spinlock(&mfh->mfh_lock);
	rc = msl_flush_int_locked(mfh, 0);
	freelock(&mfh->mfh_lock);

	DEBUG_FCMH(PLL_DIAG, mfh->mfh_fcmh,
	    "done flushing (mfh=%p, rc=%d)", mfh, rc);

	pscfs_reply_flush(pfr, rc);
	OPSTAT_INCR(SLC_OPST_FLUSH_DONE);
}

void
mfh_incref(struct msl_fhent *mfh)
{
	int lk;

	lk = MFH_RLOCK(mfh);
	mfh->mfh_refcnt++;
	MFH_URLOCK(mfh, lk);
}

void
mfh_decref(struct msl_fhent *mfh)
{
	(void)MFH_RLOCK(mfh);
	psc_assert(mfh->mfh_refcnt > 0);
	if (--mfh->mfh_refcnt == 0) {
		fcmh_op_done_type(mfh->mfh_fcmh, FCMH_OPCNT_OPEN);
		psc_pool_return(mfh_pool, mfh);
	} else
		MFH_ULOCK(mfh);
}

int
msl_flush_attr(struct fidc_membh *f)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_setattr_req *mq;
	struct srm_setattr_rep *mp;
	int rc;

	MSL_RMC_NEWREQ_PFCC(NULL, f, csvc, SRMT_SETATTR, rq, mq, mp,
	    rc);
	if (rc)
		return (rc);

	FCMH_LOCK(f);
	mq->attr.sst_fg = f->fcmh_fg;
	mq->attr.sst_size = f->fcmh_sstb.sst_size;
	mq->attr.sst_mtim = f->fcmh_sstb.sst_mtim;
	FCMH_ULOCK(f);

	mq->to_set = PSCFS_SETATTRF_FLUSH | PSCFS_SETATTRF_MTIME |
	    PSCFS_SETATTRF_DATASIZE;

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc == 0)
		rc = mp->rc;
	DEBUG_SSTB(PLL_DIAG, &f->fcmh_sstb, "attr flush, set=%x, rc=%d",
	    mq->to_set, rc);
	pscrpc_req_finished(rq);
	sl_csvc_decref(csvc);
	return (rc);
}

void
slc_getprog(pid_t pid, char fn[])
{
	char buf[PATH_MAX];

	fn[0] = '\0';
	snprintf(buf, sizeof(buf), "/proc/%d/exe", pid);
	readlink(buf, fn, PATH_MAX);
}

/**
 * mslfsop_close - This is not the same as close(2).
 */
void
mslfsop_close(struct pscfs_req *pfr, void *data)
{
	struct msl_fhent *mfh = data;
	struct fcmh_cli_info *fci;
	struct fidc_membh *c;
	int rc = 0, flush_attrs = 0;

	msfsthr_ensure();
	OPSTAT_INCR(SLC_OPST_CLOSE);

	c = mfh->mfh_fcmh;

	MFH_LOCK(mfh);
	mfh->mfh_flags |= MSL_FHENT_CLOSING;
#if FHENT_EARLY_RELEASE
	struct bmpc_ioreq *r;

	PLL_FOREACH(r, &mfh->mfh_biorqs)
		BIORQ_SETATTR(r, BIORQ_NOFHENT);
#else
	rc = msl_flush_int_locked(mfh, 1);
	psc_assert(pll_empty(&mfh->mfh_biorqs));
#endif
	while (!pll_empty(&mfh->mfh_ra_bmpces) ||
	    (mfh->mfh_flags & MSL_FHENT_RASCHED)) {
		psc_waitq_wait(&c->fcmh_waitq, &mfh->mfh_lock);
		MFH_LOCK(mfh);
	}

	/*
	 * Perhaps this checking should only be done on the mfh, with
	 * which we have modified the attributes.
	 */
	FCMH_WAIT_BUSY(c);
	if (c->fcmh_flags & FCMH_CLI_DIRTY_ATTRS) {
		flush_attrs = 1;
		c->fcmh_flags &= ~FCMH_CLI_DIRTY_ATTRS;
	}
	FCMH_ULOCK(c);

	fci = fcmh_2_fci(c);
	if (flush_attrs) {
		rc = msl_flush_attr(c);
		FCMH_LOCK(c);
		fcmh_wake_locked(c);
		if (rc) {
			c->fcmh_flags |= FCMH_CLI_DIRTY_ATTRS;
		} else if (!(c->fcmh_flags & FCMH_CLI_DIRTY_ATTRS)) {
			psc_assert(c->fcmh_flags & FCMH_CLI_DIRTY_QUEUE);
			c->fcmh_flags &= ~FCMH_CLI_DIRTY_QUEUE;
			lc_remove(&attrTimeoutQ, fci);
			fcmh_op_done_type(c, FCMH_OPCNT_DIRTY_QUEUE);
		}
	}

	if (!fcmh_isdir(c) && (mfh->mfh_nbytes_rd ||
	    mfh->mfh_nbytes_wr)) {
		char fn[PATH_MAX];

		slc_getprog(mfh->mfh_pid, fn);
		psclogs(PLL_INFO, SLCSS_INFO,
		    "file closed fid="SLPRI_FID" "
		    "uid=%u gid=%u "
		    "fsize=%"PRId64" "
		    "oatime="PFLPRI_PTIMESPEC" "
		    "mtime="PFLPRI_PTIMESPEC" sessid=%d "
		    "otime="PSCPRI_TIMESPEC" "
		    "rd=%"PSCPRIdOFFT" wr=%"PSCPRIdOFFT" prog=%s",
		    fcmh_2_fid(c),
		    c->fcmh_sstb.sst_uid, c->fcmh_sstb.sst_gid,
		    c->fcmh_sstb.sst_size,
		    PFLPRI_PTIMESPEC_ARGS(&mfh->mfh_open_atime),
		    PFLPRI_PTIMESPEC_ARGS(&c->fcmh_sstb.sst_mtim),
		    getsid(mfh->mfh_pid),
		    PSCPRI_TIMESPEC_ARGS(&mfh->mfh_open_time),
		    mfh->mfh_nbytes_rd, mfh->mfh_nbytes_wr, fn);
	}

	pscfs_reply_close(pfr, rc);

	FCMH_UNBUSY(c);
	mfh_decref(mfh);
	OPSTAT_INCR(SLC_OPST_CLOSE_DONE);
}

void
mslfsop_rename(struct pscfs_req *pfr, pscfs_inum_t opinum,
    const char *oldname, pscfs_inum_t npinum, const char *newname)
{
	struct fidc_membh *child = NULL, *np = NULL, *op = NULL, *ch;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srt_stat srcsstb, dstsstb;
	struct slash_fidgen srcfg, dstfg;
	struct srm_rename_req *mq;
	struct srm_rename_rep *mp;
	struct pscfs_creds pcr;
	struct iovec iov[2];
	int sticky, rc;

	memset(&dstsstb, 0, sizeof(dstsstb));
	srcfg.fg_fid = FID_ANY;

	msfsthr_ensure();
	OPSTAT_INCR(SLC_OPST_RENAME);

#if 0
	if (strcmp(oldname, ".") == 0 ||
	    strcmp(oldname, "..") == 0) {
		rc = EINVAL;
		goto out;
	}
#endif

	if (strlen(oldname) == 0 ||
	    strlen(newname) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(oldname) > SL_NAME_MAX ||
	    strlen(newname) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	pscfs_getcreds(pfr, &pcr);

	rc = msl_load_fcmh(pfr, opinum, &op);
	if (rc)
		PFL_GOTOERR(out, rc);

	rc = fcmh_reserved(op);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (pcr.pcr_uid) {
		FCMH_LOCK(op);
		sticky = op->fcmh_sstb.sst_mode & S_ISVTX;
		if (sticky) {
			if (op->fcmh_sstb.sst_uid == pcr.pcr_uid)
				sticky = 0;
		} else
			rc = fcmh_checkcreds(op, &pcr, W_OK);
		FCMH_ULOCK(op);
		if (rc)
			PFL_GOTOERR(out, rc);

		if (sticky) {
			rc = msl_lookup_fidcache(pfr, &pcr, opinum,
			    oldname, &srcfg, &srcsstb, &child);
			if (rc)
				PFL_GOTOERR(out, rc);
			if (srcsstb.sst_uid != pcr.pcr_uid)
				PFL_GOTOERR(out, rc = EPERM);
		}
	}

	if (npinum == opinum) {
		np = op;
	} else {
		rc = msl_load_fcmh(pfr, npinum, &np);
		if (rc)
			PFL_GOTOERR(out, rc);

		rc = fcmh_reserved(np);
		if (rc)
			PFL_GOTOERR(out, rc);

		if (pcr.pcr_uid) {
			FCMH_LOCK(np);
			sticky = np->fcmh_sstb.sst_mode & S_ISVTX;
			if (sticky) {
				if (np->fcmh_sstb.sst_uid == pcr.pcr_uid)
					sticky = 0;
			} else
				rc = fcmh_checkcreds(np, &pcr, W_OK);
			FCMH_ULOCK(np);
			if (rc)
				PFL_GOTOERR(out, rc);

			if (sticky) {
				/* XXX race */
				rc = msl_lookup_fidcache(pfr, &pcr,
				    npinum, newname, &dstfg, &dstsstb,
				    NULL);
				if (rc == 0 &&
				    dstsstb.sst_uid != pcr.pcr_uid)
					rc = EPERM;
				else
					rc = 0;
				if (rc)
					PFL_GOTOERR(out, rc);
			}
		}
	}

	if (pcr.pcr_uid) {
		if (srcfg.fg_fid == FID_ANY) {
			rc = msl_lookup_fidcache(pfr, &pcr, opinum,
			    oldname, &srcfg, &srcsstb, &child);
			if (rc)
				PFL_GOTOERR(out, rc);
		}
		if (S_ISDIR(srcsstb.sst_mode))
			rc = checkcreds(&srcsstb, &pcr, W_OK);
		if (rc)
			PFL_GOTOERR(out, rc);
	}

 retry:
	MSL_RMC_NEWREQ(pfr, np, csvc, SRMT_RENAME, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->opfg.fg_fid = opinum;
	mq->npfg.fg_fid = npinum;
	mq->opfg.fg_gen = mq->npfg.fg_gen = FGEN_ANY;
	mq->fromlen = strlen(oldname);
	mq->tolen = strlen(newname);

	if (mq->fromlen + mq->tolen > SRM_RENAME_NAMEMAX) {
		iov[0].iov_base = (char *)oldname;
		iov[0].iov_len = mq->fromlen;
		iov[1].iov_base = (char *)newname;
		iov[1].iov_len = mq->tolen;

		slrpc_bulkclient(rq, BULK_GET_SOURCE, SRMC_BULK_PORTAL,
		    iov, 2);
	} else {
		memcpy(mq->buf, oldname, mq->fromlen);
		memcpy(mq->buf + mq->fromlen, newname, mq->tolen);
	}

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;

	psclog_diag("opfid="SLPRI_FID" npfid="SLPRI_FID" from='%s' "
	    "to='%s' rc=%d", opinum, npinum, oldname, newname, rc);

	if (rc)
		PFL_GOTOERR(out, rc);

	/* refresh old parent attributes */
	FCMH_LOCK(op);
	uidmap_int_stat(&mp->srr_opattr);
	fcmh_setattr_locked(op, &mp->srr_opattr);
	FCMH_ULOCK(op);

	if (np != op) {
		/* refresh new parent attributes */
		FCMH_LOCK(np);
		uidmap_int_stat(&mp->srr_npattr);
		fcmh_setattr_locked(np, &mp->srr_npattr);
		FCMH_ULOCK(np);
	}

	/* refresh moved file's attributes */
	if (mp->srr_cattr.sst_fid != FID_ANY &&
	    fidc_lookup_fg(&mp->srr_cattr.sst_fg, &ch) == 0) {
		uidmap_int_stat(&mp->srr_cattr);
		fcmh_setattrf(ch, &mp->srr_cattr,
		    FCMH_SETATTRF_SAVELOCAL);
		fcmh_op_done(ch);
	}

	/*
	 * Refresh clobbered file's attributes.  This file might have
	 * additional links and may not be completely destroyed so don't
	 * evict.
	 */
	if (mp->srr_clattr.sst_fid != FID_ANY &&
	    fidc_lookup_fg(&mp->srr_clattr.sst_fg, &ch) == 0) {
		uidmap_int_stat(&mp->srr_clattr);
		fcmh_setattrf(ch, &mp->srr_clattr,
		    FCMH_SETATTRF_SAVELOCAL);
		fcmh_op_done(ch);
	}

	/*
	 * XXX we do not update dstsstb in our cache if the dst was
	 * nlinks > 1 and the inode was not removed from the file system
	 * outright as a result of this rename op.
	 */

 out:
	if (child)
		fcmh_op_done(child);
	if (op)
		fcmh_op_done(op);
	if (np && np != op)
		fcmh_op_done(np);

	pscfs_reply_rename(pfr, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);

	OPSTAT_INCR(SLC_OPST_RENAME_DONE);
}

void
mslfsop_statfs(struct pscfs_req *pfr, pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_statfs_req *mq;
	struct srm_statfs_rep *mp;
	struct statvfs sfb;
	int rc;

	msfsthr_ensure();

//	checkcreds

 retry:
	MSL_RMC_NEWREQ_PFCC(NULL, NULL, csvc, SRMT_STATFS, rq, mq, mp,
	    rc);
	if (rc)
		PFL_GOTOERR(out, rc);
	mq->fid = inum;
	mq->iosid = prefIOS;
	if (rc)
		PFL_GOTOERR(out, rc);
	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	sl_internalize_statfs(&mp->ssfb, &sfb);
	sfb.f_blocks = sfb.f_blocks / MSL_FS_BLKSIZ * sfb.f_frsize;
	sfb.f_bfree = sfb.f_bfree / MSL_FS_BLKSIZ * sfb.f_frsize;
	sfb.f_bavail = sfb.f_bavail / MSL_FS_BLKSIZ * sfb.f_frsize;
	sfb.f_bsize = MSL_FS_BLKSIZ;
	sfb.f_fsid = SLASH_FSID;

 out:
	pscfs_reply_statfs(pfr, &sfb, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

void
mslfsop_symlink(struct pscfs_req *pfr, const char *buf,
    pscfs_inum_t pinum, const char *name)
{
	struct fidc_membh *c = NULL, *p = NULL;
	struct slashrpc_cservice *csvc = NULL;
	struct srm_symlink_rep *mp = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_symlink_req *mq;
	struct pscfs_creds pcr;
	struct iovec iov;
	struct stat stb;
	int rc;

	msfsthr_ensure();
	OPSTAT_INCR(SLC_OPST_SYMLINK);

	if (strlen(buf) == 0 || strlen(name) == 0)
		PFL_GOTOERR(out, rc = ENOENT);
	if (strlen(buf) >= SL_PATH_MAX ||
	    strlen(name) > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = ENAMETOOLONG);

	pscfs_getcreds(pfr, &pcr);

	rc = msl_load_fcmh(pfr, pinum, &p);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (!fcmh_isdir(p))
		PFL_GOTOERR(out, rc = ENOTDIR);
	rc = fcmh_reserved(p);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, p, csvc, SRMT_SYMLINK, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->sstb.sst_uid = pcr.pcr_uid;
	mq->sstb.sst_gid = pcr.pcr_gid;
	rc = uidmap_ext_stat(&mq->sstb);
	if (rc)
		PFL_GOTOERR(out, rc);
	mq->pfg.fg_fid = pinum;
	mq->pfg.fg_gen = FGEN_ANY;
	mq->linklen = strlen(buf);
	strlcpy(mq->name, name, sizeof(mq->name));

	iov.iov_base = (char *)buf;
	iov.iov_len = mq->linklen;

	slrpc_bulkclient(rq, BULK_GET_SOURCE, SRMC_BULK_PORTAL, &iov, 1);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0)
		rc = mp->rc;
	if (rc)
		PFL_GOTOERR(out, rc);

	uidmap_int_stat(&mp->pattr);
	fcmh_setattr(p, &mp->pattr);

	uidmap_int_stat(&mp->cattr);
	rc = msl_create_fcmh(pfr, &mp->cattr, FCMH_SETATTRF_NONE, &c);

	sl_internalize_stat(&mp->cattr, &stb);

 out:
	if (c)
		fcmh_op_done(c);
	if (p)
		fcmh_op_done(p);

	pscfs_reply_symlink(pfr, mp ? mp->cattr.sst_fid : 0,
	    mp ? mp->cattr.sst_gen : 0, pscfs_entry_timeout, &stb,
	    pscfs_attr_timeout, rc);

	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

struct msl_dc_inv_entry_data {
	struct pscfs_req	*mdie_pfr;
	pscfs_inum_t		 mdie_pinum;
};

void
msl_dc_inv_entry(struct dircache_page *p, struct dircache_ent *d,
    void *arg)
{
	const struct msl_dc_inv_entry_data *mdie = arg;

	if (p->dcp_flags & DIRCACHEPGF_LOADING)
		return;

	pscfs_notify_inval_entry(mdie->mdie_pfr, mdie->mdie_pinum,
	    d->dce_name, d->dce_namelen);
}

int
inprocgrouplist(gid_t key, struct pscfs_creds *pcr)
{
	int i;

	for (i = 0; i < pcr->pcr_ngid; i++)
		if (pcr->pcr_gidv[i] == key)
			return (1);
	return (0);
}

void
mslfsop_setattr(struct pscfs_req *pfr, pscfs_inum_t inum,
    struct stat *stb, int to_set, void *data)
{
	int rc = 0, unset_trunc = 0, getting_attrs = 0, flush_attrs = 0;
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct msl_fhent *mfh = data;
	struct fidc_membh *c = NULL;
	struct srm_setattr_req *mq;
	struct srm_setattr_rep *mp;
	struct fcmh_cli_info *fci;
	struct pscfs_creds pcr;
	struct timespec ts;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_SETATTR);

	if ((to_set & PSCFS_SETATTRF_UID) && stb->st_uid == (uid_t)-1)
		to_set &= ~PSCFS_SETATTRF_UID;
	if ((to_set & PSCFS_SETATTRF_GID) && stb->st_gid == (gid_t)-1)
		to_set &= ~PSCFS_SETATTRF_GID;

	rc = msl_load_fcmh(pfr, inum, &c);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (mfh)
		psc_assert(c == mfh->mfh_fcmh);

	FCMH_WAIT_BUSY(c);

	if (to_set == 0)
		goto out;

	pscfs_getcreds(pfr, &pcr);

	if ((to_set & PSCFS_SETATTRF_MODE) && pcr.pcr_uid) {
#if 0
		if ((stb->st_mode & ALLPERMS) !=
		    (c->fcmh_sstb.sst_mode & ALLPERMS)) {
			rc = EINVAL;
			goto out;
		}
#endif
		if (pcr.pcr_uid != c->fcmh_sstb.sst_uid)
			PFL_GOTOERR(out, rc = EPERM);
		if (pcr.pcr_gid != c->fcmh_sstb.sst_gid &&
		    !inprocgrouplist(c->fcmh_sstb.sst_gid, &pcr))
			stb->st_mode &= ~S_ISGID;
	}
	if (to_set & PSCFS_SETATTRF_DATASIZE) {
		rc = fcmh_checkcreds(c, &pcr, W_OK);
		if (rc)
			PFL_GOTOERR(out, rc);
	}
	if ((to_set & (PSCFS_SETATTRF_ATIME | PSCFS_SETATTRF_MTIME)) &&
	    pcr.pcr_uid && pcr.pcr_uid != c->fcmh_sstb.sst_uid)
		PFL_GOTOERR(out, rc = EPERM);

	if (to_set & PSCFS_SETATTRF_ATIME_NOW)
	    stb->st_ctim = stb->st_atim;
	else if (to_set & PSCFS_SETATTRF_MTIME_NOW)
	    stb->st_ctim = stb->st_mtim;
	else
	    PFL_GETPTIMESPEC(&stb->st_ctim);
	to_set |= PSCFS_SETATTRF_CTIME;

	if (to_set & PSCFS_SETATTRF_UID) {
		if (pcr.pcr_uid &&
		    (pcr.pcr_uid != c->fcmh_sstb.sst_uid ||
		     pcr.pcr_uid != stb->st_uid))
			PFL_GOTOERR(out, rc = EPERM);
		// XXX sysctl fs.posix.setuid
		if (c->fcmh_sstb.sst_mode & (S_ISGID | S_ISUID)) {
			to_set |= PSCFS_SETATTRF_MODE;
			stb->st_mode = c->fcmh_sstb.sst_mode &
			    ~(S_ISGID | S_ISUID);
		}
	}
	if (to_set & PSCFS_SETATTRF_GID) {
		if (pcr.pcr_uid &&
		    (pcr.pcr_uid != c->fcmh_sstb.sst_uid ||
		     !inprocgrouplist(stb->st_gid, &pcr)))
			PFL_GOTOERR(out, rc = EPERM);
		// XXX sysctl fs.posix.setuid
		if (c->fcmh_sstb.sst_mode & (S_ISGID | S_ISUID)) {
			to_set |= PSCFS_SETATTRF_MODE;
			stb->st_mode = c->fcmh_sstb.sst_mode &
			    ~(S_ISGID | S_ISUID);
		}
	}

 wait_trunc_res:
	if (to_set & PSCFS_SETATTRF_DATASIZE) {
		fcmh_wait_locked(c, c->fcmh_flags & FCMH_CLI_TRUNC);
		/*
		 * Mark as busy against I/O on this and higher bmaps and
		 * concurrent truncation requests util the MDS has
		 * received new CRCs for the freshly truncated region.
		 */
		c->fcmh_flags |= FCMH_CLI_TRUNC;
		unset_trunc = 1;
	}

	if (to_set & PSCFS_SETATTRF_DATASIZE) {
		struct psc_dynarray a = DYNARRAY_INIT;
		struct bmapc_memb *b;
		int j;

		if (!stb->st_size) {
			DEBUG_FCMH(PLL_DIAG, c,
			   "full truncate, free bmaps");

			OPSTAT_INCR(SLC_OPST_TRUNCATE_FULL);
			bmap_free_all_locked(c);
			FCMH_ULOCK(c);

		} else if (stb->st_size == (ssize_t)fcmh_2_fsz(c)) {
			/*
			 * No-op.  Don't send truncate request if the
			 * sizes match.
			 */
			goto out;

		} else {
			uint32_t x = stb->st_size / SLASH_BMAP_SIZE;

			OPSTAT_INCR(SLC_OPST_TRUNCATE_PART);

			DEBUG_FCMH(PLL_DIAG, c, "partial truncate");

			/* Partial truncate.  Block and flush. */
			SPLAY_FOREACH(b, bmap_cache, &c->fcmh_bmaptree) {
				if (b->bcm_bmapno < x)
					continue;

				/*
				 * Take a reference to ensure the bmap
				 * is still valid.
				 * bmap_biorq_waitempty() shoudn't be
				 * called while holding the fcmh lock.
				 */
				bmap_op_start_type(b,
				    BMAP_OPCNT_TRUNCWAIT);
				DEBUG_BMAP(PLL_DIAG, b,
				    "BMAP_OPCNT_TRUNCWAIT");
				psc_dynarray_add(&a, b);
			}
			FCMH_ULOCK(c);

			/*
			 * XXX some writes can be cancelled, but no api
			 * exists yet.
			 */
			DYNARRAY_FOREACH(b, j, &a)
				bmap_biorq_expire(b);

			DYNARRAY_FOREACH(b, j, &a) {
				bmap_biorq_waitempty(b);
				psc_assert(atomic_read(&b->bcm_opcnt) == 1);
				bmap_op_done_type(b, BMAP_OPCNT_TRUNCWAIT);
			}
		}
		psc_dynarray_free(&a);

	}

	(void)FCMH_RLOCK(c);
	/* We're obtaining the attributes now. */
	if ((c->fcmh_flags & (FCMH_GETTING_ATTRS | FCMH_HAVE_ATTRS)) == 0) {
		getting_attrs = 1;
		c->fcmh_flags |= FCMH_GETTING_ATTRS;
	}
	FCMH_ULOCK(c);

	/*
	 * Turn on mtime explicitly if we are going to change the size.
	 * We want our local time to be saved, not the time when the RPC
	 * arrives at the MDS.
	 */
	if ((to_set & PSCFS_SETATTRF_DATASIZE) &&
	    !(to_set & PSCFS_SETATTRF_MTIME)) {
		to_set |= PSCFS_SETATTRF_MTIME;
		PFL_GETTIMESPEC(&ts);
		PFL_STB_MTIME_SET(ts.tv_sec, ts.tv_nsec, stb);
	}

 retry:
	MSL_RMC_NEWREQ(pfr, c, csvc, SRMT_SETATTR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	FCMH_LOCK(c);
	/* No need to do this on retry. */
	if (c->fcmh_flags & FCMH_CLI_DIRTY_ATTRS) {
		flush_attrs = 1;
		to_set |= PSCFS_SETATTRF_FLUSH;
		if (!(to_set & PSCFS_SETATTRF_MTIME)) {
			to_set |= PSCFS_SETATTRF_MTIME;
			PFL_STB_MTIME_SET(c->fcmh_sstb.sst_mtime,
					  c->fcmh_sstb.sst_mtime_ns,
					  stb);
		}
		if (!(to_set & PSCFS_SETATTRF_DATASIZE)) {
			to_set |= PSCFS_SETATTRF_DATASIZE;
			stb->st_size = c->fcmh_sstb.sst_size;
		}
		c->fcmh_flags &= ~FCMH_CLI_DIRTY_ATTRS;
	}

	mq->attr.sst_fg = c->fcmh_fg;
	mq->to_set = to_set;
	sl_externalize_stat(stb, &mq->attr);
	uidmap_ext_stat(&mq->attr);

	DEBUG_SSTB(PLL_DIAG, &c->fcmh_sstb,
	    "fcmh %p pre setattr, set = %#x", c, to_set);

	psclog_debug("fcmh %p setattr%s%s%s%s%s%s%s", c,
	    to_set & PSCFS_SETATTRF_MODE ? " mode" : "",
	    to_set & PSCFS_SETATTRF_UID ? " uid" : "",
	    to_set & PSCFS_SETATTRF_GID ? " gid" : "",
	    to_set & PSCFS_SETATTRF_ATIME ? " atime" : "",
	    to_set & PSCFS_SETATTRF_MTIME ? " mtime" : "",
	    to_set & PSCFS_SETATTRF_CTIME ? " ctime" : "",
	    to_set & PSCFS_SETATTRF_DATASIZE ? " datasize" : "");

	FCMH_ULOCK(c);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0) {
		switch (mp->rc) {
		case -SLERR_BMAP_IN_PTRUNC:
			if (getting_attrs) {
				getting_attrs = 0;
				FCMH_LOCK(c);
				c->fcmh_flags &= ~FCMH_GETTING_ATTRS;
			}
			goto wait_trunc_res;
		case -SLERR_BMAP_PTRUNC_STARTED:
			unset_trunc = 0;
			rc = 0;
			break;
		default:
			rc = -mp->rc;
			break;
		}
	}
	if (rc)
		PFL_GOTOERR(out, rc);

	FCMH_LOCK(c);

	/*
	 * If we are setting mtime or size, we told the MDS what we
	 * wanted it to be and must now blindly accept what he returns
	 * to us; otherwise, we SAVELOCAL any updates we've made.
	 */
	if (to_set & (PSCFS_SETATTRF_MTIME | PSCFS_SETATTRF_DATASIZE)) {
		c->fcmh_sstb.sst_mtime = mp->attr.sst_mtime;
		c->fcmh_sstb.sst_mtime_ns = mp->attr.sst_mtime_ns;
	}

	if (to_set & PSCFS_SETATTRF_DATASIZE) {
		if (c->fcmh_sstb.sst_size != mp->attr.sst_size)
			psclog_info("fid: "SLPRI_FID", size change "
			    "from %"PRId64" to %"PRId64,
			    fcmh_2_fid(c), c->fcmh_sstb.sst_size,
			    mp->attr.sst_size);
		c->fcmh_sstb.sst_size = mp->attr.sst_size;
		c->fcmh_sstb.sst_ctime = mp->attr.sst_ctime;
		c->fcmh_sstb.sst_ctime_ns = mp->attr.sst_ctime_ns;
	}

	uidmap_int_stat(&mp->attr);
	fcmh_setattrf(c, &mp->attr, FCMH_SETATTRF_SAVELOCAL |
	    FCMH_SETATTRF_HAVELOCK);

	DEBUG_SSTB(PLL_DIAG, &c->fcmh_sstb, "fcmh %p post setattr", c);

#if 0
	if (fcmh_isdir(c)) {
		struct msl_dc_inv_entry_data mdie;

		mdie.mdie_pfr = pfr;
		mdie.mdie_pinum = fcmh_2_fid(c);
		/* XXX this currently crashes fuse.ko but needs to happen */
		dircache_walk(fcmh_2_dci(c), msl_dc_inv_entry, &mdie);
	}
#endif

 out:
	if (c) {
		(void)FCMH_RLOCK(c);
		if (unset_trunc) {
			c->fcmh_flags &= ~FCMH_CLI_TRUNC;
			fcmh_wake_locked(c);
		}
		if (rc && getting_attrs)
			c->fcmh_flags &= ~FCMH_GETTING_ATTRS;
		sl_internalize_stat(&c->fcmh_sstb, stb);

		if (rc && flush_attrs)
			c->fcmh_flags |= FCMH_CLI_DIRTY_ATTRS;
		if (!rc && flush_attrs &&
		    !(c->fcmh_flags & FCMH_CLI_DIRTY_ATTRS)) {
			psc_assert(c->fcmh_flags & FCMH_CLI_DIRTY_QUEUE);
			c->fcmh_flags &= ~FCMH_CLI_DIRTY_QUEUE;

			fci = fcmh_2_fci(c);
			lc_remove(&attrTimeoutQ, fci);

			fcmh_op_done_type(c, FCMH_OPCNT_DIRTY_QUEUE);
		}
		FCMH_UNBUSY(c);
		fcmh_op_done(c);
	}
	/* XXX if there is no fcmh, what do we do?? */
	pscfs_reply_setattr(pfr, stb, pscfs_attr_timeout, rc);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);

	OPSTAT_INCR(SLC_OPST_SETATTR_DONE);
}

void
mslfsop_fsync(struct pscfs_req *pfr, __unusedx int datasync, void *data)
{
	struct msl_fhent *mfh;
	int rc;

	mfh = data;
	OPSTAT_INCR(SLC_OPST_FSYNC);

	DEBUG_FCMH(PLL_DIAG, mfh->mfh_fcmh, "fsyncing via flush");

	spinlock(&mfh->mfh_lock);
	rc = msl_flush_int_locked(mfh, 1);
	freelock(&mfh->mfh_lock);

	pscfs_reply_fsync(pfr, rc);
	OPSTAT_INCR(SLC_OPST_FSYNC_DONE);
}

void
mslfsop_umount(void)
{
	pscthr_killall();
	pscrpc_exit_portals();
	/* XXX wait */
//	unmount_mp();
	exit(0);
}

void
mslfsop_write(struct pscfs_req *pfr, const void *buf, size_t size,
    off_t off, void *data)
{
	struct msl_fhent *mfh = data;
	struct fidc_membh *f;
	int rc = 0;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_WRITE);

	f = mfh->mfh_fcmh;

	/* XXX EBADF if fd is not open for writing */
	if (fcmh_isdir(f))
		PFL_GOTOERR(out, rc = EISDIR);
	if (!size)
		goto out;

	rc = msl_write(pfr, mfh, buf, size, off);

 out:
	if (rc) {
psc_assert(rc != 1);
		pscfs_reply_write(pfr, size, rc);
		OPSTAT_INCR(SLC_OPST_FSRQ_WRITE_FREE);
	}
	DEBUG_FCMH(PLL_DIAG, f, "write: buf=%p rc=%d sz=%zu "
	    "off=%"PSCPRIdOFFT, buf, rc, size, off);

	OPSTAT_INCR(SLC_OPST_WRITE_DONE);
}

void
mslfsop_read(struct pscfs_req *pfr, size_t size, off_t off, void *data)
{
	struct msl_fhent *mfh = data;
	struct fidc_membh *f;
	void *buf = pfr->pfr_buf;
	ssize_t len = 0;
	int rc = 0;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_READ);

	f = mfh->mfh_fcmh;

	DEBUG_FCMH(PLL_DIAG, f, "read (start): buf=%p rc=%d sz=%zu "
	    "len=%zd off=%"PSCPRIdOFFT, buf, rc, size, len, off);

	if (fcmh_isdir(f)) {
//		psclog_errorx("regular file is a directory");
		PFL_GOTOERR(out, rc = EISDIR);
	}
	if (!size)
		goto out;

	rc = msl_read(pfr, mfh, buf, size, off);

 out:
	if (rc) {
		pscfs_reply_read(pfr, buf, len, rc);
		OPSTAT_INCR(SLC_OPST_FSRQ_READ_FREE);
	}

	DEBUG_FCMH(PLL_DIAG, f, "read (end): buf=%p rc=%d sz=%zu "
	    "len=%zd off=%"PSCPRIdOFFT, buf, rc, size, len, off);

	OPSTAT_INCR(SLC_OPST_READ_DONE);
}

void
mslfsop_listxattr(struct pscfs_req *pfr, size_t size, pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_listxattr_rep *mp = NULL;
	struct srm_listxattr_req *mq;
	struct fidc_membh *f = NULL;
	struct pscfs_creds pcr;
	struct iovec iov;
	char *buf = NULL;
	int rc;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_LISTXATTR);

	pscfs_getcreds(pfr, &pcr);

	rc = msl_load_fcmh(pfr, inum, &f);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (size)
		buf = PSCALLOC(size);

 retry:
	MSL_RMC_NEWREQ(pfr, f, csvc, SRMT_LISTXATTR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg.fg_fid = inum;
	mq->fg.fg_gen = FGEN_ANY;
	mq->size = size;

	if (size) {
		iov.iov_base = buf;
		iov.iov_len = size;
		slrpc_bulkclient(rq, BULK_PUT_SINK, SRMC_BULK_PORTAL,
		    &iov, 1);
		rq->rq_bulk_abortable = 1;
	}

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0) {
		rc = mp->rc;
		FCMH_LOCK(f);
		fcmh_2_fci(f)->fci_xattrsize = mp->size;
	}

 out:
	pscfs_reply_listxattr(pfr, buf, mp ? mp->size : 0, rc);

	if (f)
		fcmh_op_done(f);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
	PSCFREE(buf);
}

void
mslfsop_setxattr(struct pscfs_req *pfr, const char *name,
    const void *value, size_t size, pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct pscrpc_request *rq = NULL;
	struct srm_setxattr_rep *mp = NULL;
	struct srm_setxattr_req *mq;
	struct fidc_membh *f = NULL;
	struct pscfs_creds pcr;
	struct iovec iov;
	int rc;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_SETXATTR);

	if (size > SL_NAME_MAX)
		PFL_GOTOERR(out, rc = EINVAL);

	pscfs_getcreds(pfr, &pcr);
	rc = msl_load_fcmh(pfr, inum, &f);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, f, csvc, SRMT_SETXATTR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg.fg_fid = inum;
	mq->fg.fg_gen = FGEN_ANY;
	mq->namelen = strlen(name) + 1;
	mq->valuelen = size;
	memcpy(mq->name, name, mq->namelen + 1);

	iov.iov_base = (char *)value;
	iov.iov_len = mq->valuelen;

	slrpc_bulkclient(rq, BULK_GET_SOURCE, SRMC_BULK_PORTAL, &iov, 1);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;

	if (rc == 0) {
		rc = mp->rc;
		FCMH_LOCK(f);
		// XXX fix
		fcmh_2_fci(f)->fci_xattrsize = 1;
	}

 out:
	pscfs_reply_setxattr(pfr, rc);

	if (f)
		fcmh_op_done(f);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

void
mslfsop_getxattr(struct pscfs_req *pfr, const char *name,
    size_t size, pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct srm_getxattr_rep *mp = NULL;
	struct srm_getxattr_req *mq;
	struct pscrpc_request *rq = NULL;
	struct fidc_membh *f = NULL;
	struct fcmh_cli_info *fci;
	struct pscfs_creds pcr;
	struct iovec iov;
	char *buf = NULL;
	size_t retsz = 0;
	int rc;

	iov.iov_base = NULL;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_GETXATTR);

	pscfs_getcreds(pfr, &pcr);

	rc = msl_load_fcmh(pfr, inum, &f);
	if (rc)
		PFL_GOTOERR(out, rc);

	if (f->fcmh_flags & FCMH_HAVE_ATTRS) {
		struct timeval now;

		PFL_GETTIMEVAL(&now);
		fci = fcmh_2_fci(f);
		FCMH_LOCK(f);
		if (timercmp(&now, &fci->fci_age, <) &&
		    fci->fci_xattrsize == 0)
			PFL_GOTOERR(out, rc = ENODATA);
		FCMH_ULOCK(f);
	}

	if (size)
		buf = PSCALLOC(size);

 retry:
	MSL_RMC_NEWREQ(pfr, f, csvc, SRMT_GETXATTR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg.fg_fid = inum;
	mq->fg.fg_gen = FGEN_ANY;
	mq->size = size;
	mq->namelen = strlen(name) + 1;
	memcpy(mq->name, name, mq->namelen + 1);

	if (size) {
		iov.iov_base = buf;
		iov.iov_len = size;
		slrpc_bulkclient(rq, BULK_PUT_SINK, SRMC_BULK_PORTAL,
		    &iov, 1);
		rq->rq_bulk_abortable = 1;
	}

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;
	if (rc == 0) {
		rc = mp->rc;
		retsz = mp->valuelen;
	}

 out:
	/*
	 * If MDS does not support this, we return no attributes
	 * successfully.
	 */
#if 0
	if (rc == -PFLERR_NOSYS) {
		OPSTAT_INCR(SLC_OPST_GETXATTR_NOSYS);
		rc = 0;
	}
#endif
	pscfs_reply_getxattr(pfr, buf, retsz, rc);

	if (f)
		fcmh_op_done(f);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);

	PSCFREE(buf);
}

void
mslfsop_removexattr(struct pscfs_req *pfr, const char *name,
    pscfs_inum_t inum)
{
	struct slashrpc_cservice *csvc = NULL;
	struct srm_removexattr_rep *mp = NULL;
	struct srm_removexattr_req *mq;
	struct pscrpc_request *rq = NULL;
	struct fidc_membh *f = NULL;
	struct pscfs_creds pcr;
	int rc;

	msfsthr_ensure();

	OPSTAT_INCR(SLC_OPST_REMOVEXATTR);

	pscfs_getcreds(pfr, &pcr);

	rc = msl_load_fcmh(pfr, inum, &f);
	if (rc)
		PFL_GOTOERR(out, rc);

 retry:
	MSL_RMC_NEWREQ(pfr, f, csvc, SRMT_REMOVEXATTR, rq, mq, mp, rc);
	if (rc)
		PFL_GOTOERR(out, rc);

	mq->fg.fg_fid = inum;
	mq->fg.fg_gen = FGEN_ANY;
	mq->namelen = strlen(name) + 1;
	memcpy(mq->name, name, mq->namelen + 1);

	rc = SL_RSX_WAITREP(csvc, rq, mp);
	if (rc && slc_rmc_retry(pfr, &rc))
		goto retry;

	if (rc == 0)
		rc = mp->rc;

 out:
	pscfs_reply_removexattr(pfr, rc);

	if (f)
		fcmh_op_done(f);
	if (rq)
		pscrpc_req_finished(rq);
	if (csvc)
		sl_csvc_decref(csvc);
}

void
msattrflushthr_main(struct psc_thread *thr)
{
	struct fcmh_cli_info *fci, *tmp_fci;
	struct timespec ts, nexttimeo;
	struct fidc_membh *f;
	int rc, did_work;

	while (pscthr_run(thr)) {

		lc_peekheadwait(&attrTimeoutQ);

		did_work = 0;
		PFL_GETTIMESPEC(&ts);
		nexttimeo.tv_sec = FCMH_ATTR_TIMEO;
		nexttimeo.tv_nsec = 0;

		LIST_CACHE_LOCK(&attrTimeoutQ);
		LIST_CACHE_FOREACH_SAFE(fci, tmp_fci, &attrTimeoutQ) {

			f = fci_2_fcmh(fci);
			if (!FCMH_TRYLOCK(f))
				continue;
			if (f->fcmh_flags & FCMH_BUSY) {
				FCMH_ULOCK(f);
				continue;
			}
			psc_assert(f->fcmh_flags & FCMH_CLI_DIRTY_ATTRS);

			if (fci->fci_etime.tv_sec > ts.tv_sec ||
			   (fci->fci_etime.tv_sec == ts.tv_sec &&
			    fci->fci_etime.tv_nsec > ts.tv_nsec)) {
				timespecsub(&fci->fci_etime, &ts,
				    &nexttimeo);
				FCMH_ULOCK(f);
				break;
			}
			FCMH_WAIT_BUSY(f);
			f->fcmh_flags &= ~FCMH_CLI_DIRTY_ATTRS;
			FCMH_ULOCK(f);

			LIST_CACHE_ULOCK(&attrTimeoutQ);

			OPSTAT_INCR(SLC_OPST_FLUSH_ATTR);

			rc = msl_flush_attr(f);

			FCMH_LOCK(f);
			if (rc) {
				f->fcmh_flags |= FCMH_CLI_DIRTY_ATTRS;
				FCMH_UNBUSY(f);
			} else if (!(f->fcmh_flags & FCMH_CLI_DIRTY_ATTRS)) {
				psc_assert(f->fcmh_flags & FCMH_CLI_DIRTY_QUEUE);
				f->fcmh_flags &= ~FCMH_CLI_DIRTY_QUEUE;
				lc_remove(&attrTimeoutQ, fci);
				FCMH_UNBUSY(f);
				fcmh_op_done_type(f, FCMH_OPCNT_DIRTY_QUEUE);
			} else
				FCMH_UNBUSY(f);

			did_work = 1;
			break;
		}
		if (did_work)
			continue;
		else
			LIST_CACHE_ULOCK(&attrTimeoutQ);

		OPSTAT_INCR(SLC_OPST_FLUSH_ATTR_WAIT);
		spinlock(&msl_flush_attrqlock);
		psc_waitq_waitrel(&msl_flush_attrq,
		    &msl_flush_attrqlock, &nexttimeo);
	}
}

void
msattrflushthr_spawn(void)
{
	struct msattrfl_thread *mattrft;
	struct psc_thread *thr;

	lc_reginit(&attrTimeoutQ, struct fcmh_cli_info,
	    fci_lentry, "attrtimeout");

	thr = pscthr_init(MSTHRT_ATTRFLSH, 0, msattrflushthr_main, NULL,
	    sizeof(struct msattrfl_thread), "msattrflushthr");
	mattrft = msattrflthr(thr);
	psc_multiwait_init(&mattrft->maft_mw, "%s",
	    thr->pscthr_name);
	pscthr_setready(thr);
}

void
unmount(const char *mp)
{
	char buf[BUFSIZ];
	int rc;

	/* XXX do not let this hang */
	rc = snprintf(buf, sizeof(buf),
	    "umount '%s' || umount -f '%s' || umount -l '%s'",
	    mp, mp, mp);
	if (rc == -1)
		psc_fatal("snprintf: umount %s", mp);
	if (rc >= (int)sizeof(buf))
		psc_fatalx("snprintf: umount %s: too long", mp);
	if (system(buf) == -1)
		psclog_warn("system(%s)", buf);
}

void
unmount_mp(void)
{
	unmount(mountpoint);
}

void
msl_init(void)
{
	char *name;
	int rc;

	authbuf_checkkeyfile();
	authbuf_readkeyfile();

	libsl_init(4096);
	fidc_init(sizeof(struct fcmh_cli_info), FIDC_CLI_DEFSZ);
	bmpc_global_init();
	bmap_cache_init(sizeof(struct bmap_cli_info));
	dircache_mgr_init();

	psc_poolmaster_init(&slc_async_req_poolmaster,
	    struct slc_async_req, car_lentry, PPMF_AUTO, 64, 64, 0,
	    NULL, NULL, NULL, "asyncrq");
	slc_async_req_pool = psc_poolmaster_getmgr(&slc_async_req_poolmaster);

	psc_poolmaster_init(&slc_biorq_poolmaster,
	    struct bmpc_ioreq, biorq_lentry, PPMF_AUTO, 64, 64, 0, NULL,
	    NULL, NULL, "biorq");
	slc_biorq_pool = psc_poolmaster_getmgr(&slc_biorq_poolmaster);

	psc_poolmaster_init(&mfh_poolmaster,
	    struct msl_fhent, mfh_lentry, PPMF_AUTO, 64, 64, 0, NULL,
	    NULL, NULL, "mfh");
	mfh_pool = psc_poolmaster_getmgr(&mfh_poolmaster);

	pndgReadaReqs = pscrpc_nbreqset_init(NULL, NULL);

	pfl_workq_init(128);
	pfl_wkthr_spawn(MSTHRT_WORKER, 4, "mswkthr%d");

	slrpc_initcli();
	slc_rpc_initsvc();

	/* Start up service threads. */
	psc_eqpollthr_spawn(MSTHRT_EQPOLL, "mseqpollthr");
	msctlthr_spawn();
	mstimerthr_spawn();

	psc_iostats_init(&msl_diord_stat, "dio-rd");
	psc_iostats_init(&msl_diowr_stat, "dio-wr");
	psc_iostats_init(&msl_rdcache_stat, "rd-cache-hit");
	psc_iostats_init(&msl_racache_stat, "ra-cache-hit");

	psc_iostats_initf(&msl_io_1b_stat, PISTF_BASE10, "iosz:0-1k");
	psc_iostats_initf(&msl_io_1k_stat, PISTF_BASE10, "iosz:1k-3k");
	psc_iostats_initf(&msl_io_4k_stat, PISTF_BASE10, "iosz:4k-15k");
	psc_iostats_initf(&msl_io_16k_stat, PISTF_BASE10, "iosz:16k-63k");
	psc_iostats_initf(&msl_io_64k_stat, PISTF_BASE10, "iosz:64k-127k");
	psc_iostats_initf(&msl_io_128k_stat, PISTF_BASE10, "iosz:128k-511k");
	psc_iostats_initf(&msl_io_512k_stat, PISTF_BASE10, "iosz:512k-1m");
	psc_iostats_initf(&msl_io_1m_stat, PISTF_BASE10, "iosz:1m-");

	sl_nbrqset = pscrpc_nbreqset_init(NULL, NULL);
	pscrpc_nbreapthr_spawn(sl_nbrqset, MSTHRT_NBRQ, "msnbrqthr");

	msattrflushthr_spawn();
	msbmapflushthr_spawn();

	if ((name = getenv("SLASH_MDS_NID")) == NULL)
		psc_fatalx("SLASH_MDS_NID not specified");

	rc = slc_rmc_setmds(name);
	if (rc)
		psc_fatalx("invalid MDS %s: %s", name, slstrerror(rc));

	name = getenv("SLASH2_PIOS_ID");
	if (name) {
		prefIOS = libsl_str2id(name);
		if (prefIOS == IOS_ID_ANY)
			psclog_warnx("SLASH2_PIOS_ID (%s) does not resolve to "
			    "a valid IOS; defaulting to IOS_ID_ANY", name);
	}
	atexit(unmount_mp);
}

struct pscfs pscfs = {
	mslfsop_access,
	mslfsop_close,
	mslfsop_close,		/* closedir */
	mslfsop_create,
	mslfsop_flush,
	mslfsop_fsync,
	mslfsop_fsync,
	mslfsop_getattr,
	NULL,			/* ioctl */
	mslfsop_link,
	mslfsop_lookup,
	mslfsop_mkdir,
	mslfsop_mknod,
	mslfsop_open,
	mslfsop_opendir,
	mslfsop_read,
	mslfsop_readdir,
	mslfsop_readlink,
	mslfsop_rename,
	mslfsop_rmdir,
	mslfsop_setattr,
	mslfsop_statfs,
	mslfsop_symlink,
	mslfsop_unlink,
	mslfsop_umount,
	mslfsop_write,
	mslfsop_listxattr,
	mslfsop_getxattr,
	mslfsop_setxattr,
	mslfsop_removexattr
};

int
psc_usklndthr_get_type(const char *namefmt)
{
	if (strstr(namefmt, "lnacthr"))
		psc_fatalx("invalid name");
	return (MSTHRT_USKLNDPL);
}

void
psc_usklndthr_get_namev(char buf[PSC_THRNAME_MAX], const char *namefmt,
    va_list ap)
{
	size_t n;

	n = strlcpy(buf, "ms", PSC_THRNAME_MAX);
	if (n < PSC_THRNAME_MAX)
		vsnprintf(buf + n, PSC_THRNAME_MAX - n, namefmt, ap);
}

void
parse_allowexe(void)
{
	char *p, *s, *t;
	struct stat stb;

	s = globalConfig.gconf_allowexe;
	while (s) {
		p = s;
		while (isspace(*p))
			p++;
		s = strchr(p, ':');
		if (s)
			*s++ = '\0';
		if (strlen(p)) {
			t = p + strlen(p) - 1;
			if (isspace(*t)) {
				while (isspace(*t))
					t--;
				t[1] = '\0';
			}
		}
		if (*p == '\0')
			continue;
		if (stat(p, &stb) == -1) {
			warn("%s", p);
			continue;
		}

		psc_dynarray_add(&allow_exe, p);
		psclog_info("restricting open(2) access to %s", p);
	}
}

void
parse_mapfile(void)
{
	char fn[PATH_MAX], buf[LINE_MAX], *endp, *p, *t;
	struct uid_mapping *um;
	uid_t to, from;
	FILE *fp;
	long l;
	int ln;

	xmkfn(fn, "%s/%s", sl_datadir, SL_FN_MAPFILE);

	fp = fopen(fn, "r");
	if (fp == NULL)
		err(1, "%s", fn);
	ln = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		ln++;

		for (p = t = buf; isdigit(*t); t++)
			;
		if (!isspace(*t))
			goto malformed;
		*t++ = '\0';
		while (isspace(*t))
			t++;

		l = strtol(p, &endp, 10);
		if (l < 0 || l >= INT_MAX ||
		    endp == p || *endp)
			goto malformed;
		from = l;

		for (p = t; isdigit(*t); t++)
			;
		*t++ = '\0';
		while (isspace(*t))
			t++;
		if (*t)
			goto malformed;

		l = strtol(p, &endp, 10);
		if (l < 0 || l >= INT_MAX ||
		    endp == p || *endp)
			goto malformed;
		to = l;

		um = PSCALLOC(sizeof(*um));
		psc_hashent_init(&slc_uidmap_ext, um);
		um->um_key = from;
		um->um_val = to;
		psc_hashtbl_add_item(&slc_uidmap_ext, um);

		um = PSCALLOC(sizeof(*um));
		psc_hashent_init(&slc_uidmap_int, um);
		um->um_key = to;
		um->um_val = from;
		psc_hashtbl_add_item(&slc_uidmap_int, um);

		continue;

 malformed:
		warn("%s: %d: malformed line", fn, ln);
	}
	if (ferror(fp))
		warn("%s", fn);
	fclose(fp);
}

int
opt_lookup(const char *opt)
{
	struct {
		const char	*name;
		int		*var;
	} *io, opts[] = {
		{ "mapfile",	&use_mapfile },
		{ NULL,		NULL }
	};

	for (io = opts; io->name; io++)
		if (strcmp(opt, io->name) == 0) {
			*io->var = 1;
			return (1);
		}
	return (0);
}

__dead void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-dQUVX] [-D datadir] [-f conf] [-I iosystem] [-M mds]\n"
	    "\t[-o mountopt] [-S socket] node\n",
	    progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct pscfs_args args = PSCFS_ARGS_INIT(0, NULL);
	char c, *p, *noncanon_mp, *cfg = SL_PATH_CONF;
	int unmount_first = 0;

	/* gcrypt must be initialized very early on */
	gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	if (!gcry_check_version(GCRYPT_VERSION))
		errx(1, "libgcrypt version mismatch");

	progname = argv[0];
	pfl_init();
	sl_subsys_register();
	psc_subsys_register(SLCSS_INFO, "info");

	psc_fault_register(SLC_FAULT_READAHEAD_CB_EIO);
	psc_fault_register(SLC_FAULT_READRPC_OFFLINE);
	psc_fault_register(SLC_FAULT_READ_CB_EIO);
	psc_fault_register(SLC_FAULT_REQUEST_TIMEOUT);

	pscfs_addarg(&args, "");		/* progname/argv[0] */
	pscfs_addarg(&args, "-o");
	pscfs_addarg(&args, STD_MOUNT_OPTIONS);

	p = getenv("CTL_SOCK_FILE");
	if (p)
		ctlsockfn = p;

	cfg = SL_PATH_CONF;
	p = getenv("CONFIG_FILE");
	if (p)
		cfg = p;

	while ((c = getopt(argc, argv, "D:df:I:M:o:QS:UVX")) != -1)
		switch (c) {
		case 'D':
			sl_datadir = optarg;
			break;
		case 'd':
			pscfs_addarg(&args, "-odebug");
			break;
		case 'f':
			cfg = optarg;
			break;
		case 'I':
			setenv("SLASH2_PIOS_ID", optarg, 1);
			break;
		case 'M':
			setenv("SLASH_MDS_NID", optarg, 1);
			break;
		case 'o':
			if (!opt_lookup(optarg)) {
				pscfs_addarg(&args, "-o");
				pscfs_addarg(&args, optarg);
			}
			break;
		case 'Q':
			globalConfig.gconf_root_squash = 1;
			break;
		case 'S':
			ctlsockfn = optarg;
			break;
		case 'U':
			unmount_first = 1;
			break;
		case 'V':
			errx(0, "revision is %d", SL_STK_VERSION);
		case 'X':
			allow_root_uid = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	pscthr_init(MSTHRT_FSMGR, 0, NULL, NULL, 0, "msfsmgrthr");

	sl_sys_upnonce = psc_random32();

	noncanon_mp = argv[0];
	if (unmount_first)
		unmount(noncanon_mp);

	/* canonicalize mount path */
	if (realpath(noncanon_mp, mountpoint) == NULL)
		psc_fatal("realpath %s", noncanon_mp);

	pscfs_mount(mountpoint, &args);
	pscfs_freeargs(&args);

	sl_drop_privs(allow_root_uid);

	slcfg_parse(cfg);
	parse_allowexe();
	if (use_mapfile) {
		psc_hashtbl_init(&slc_uidmap_ext, 0, struct uid_mapping,
		    um_key, um_hentry, 128, NULL, "uidmapext");
		psc_hashtbl_init(&slc_uidmap_int, 0, struct uid_mapping,
		    um_key, um_hentry, 128, NULL, "uidmapint");
		parse_mapfile();
	}
	msl_init();

	pscfs_entry_timeout = 8.;
	pscfs_attr_timeout = 8.;

	OPSTAT_ASSIGN(SLC_OPST_VERSION, SL_STK_VERSION);
	exit(pscfs_main(sizeof(struct msl_fsrqinfo)));
}
