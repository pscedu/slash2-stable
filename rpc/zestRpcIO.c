/* $Id: zestRpcIO.c 2203 2007-11-08 22:33:25Z yanovich $ */

#include <sys/time.h>

#include <stdio.h>

#include "zestConfig.h"
#include "zestCrc.h"
#include "zestInodeCache.h"
#include "zestLog.h"
#include "zestRpc.h"
#include "zestRpcIO.h"
#include "zestRpcLog.h"
#include "zestThread.h"
#include "zestThreadTable.h"
#include "ciod.h"
#include "iostats.h"
#include "cdefs.h"

struct zestrpc_service *zestRpcIoSvc;
struct iostats rpcIOStats;

int
zrpc_io_bulk_timeout(__unusedx void *arg)
{
	return 0;
}

int
zio_handle_connect(struct zestrpc_request *req)
{
	struct zio_connect_body *zicb, *repzicb;
	int rc, size;

	size = sizeof(*zicb);
	zicb = zest_msg_buf(req->rq_reqmsg, 0, size);
	if (zicb == NULL) {
		zwarnx("connect_body is null");
		rc = -1;
		goto fail;
	}
	znotify("fsmagic %"ZLPX64" version %u",
	    zicb->zicb_magic, zicb->zicb_version);

	rc = zest_pack_reply(req, 1, &size, NULL);
	if (rc) {
		zest_assert(rc == -ENOMEM);
		zerror("zest_pack_reply failed");
		goto fail;
	}

	repzicb = zest_msg_buf(req->rq_repmsg, 0, size);
	if (repzicb == NULL) {
		zwarnx("connect repzicb is null");
		rc = -1;
		goto fail;
	}

	if (zicb->zicb_magic == ZIO_CONNECT_MAGIC &&
	    zicb->zicb_version == ZIO_VERSION)
		/*
		 * Return the body back to the client
		 */
		memcpy(repzicb, zicb, size);
	else {
		repzicb->zicb_magic = 0;
		rc = -1;
		goto fail;
	}

	znotify("Connect request from %"ZLPX64":%u",
	    req->rq_peer.nid, req->rq_peer.pid);
	return 0;
 fail:
	znotify("Failed connect request from %"ZLPX64":%u",
	    req->rq_peer.nid, req->rq_peer.pid);
	return (rc);
}

__inline int
zio_handle_write_rpc(struct zestrpc_request *req, struct ciod_ingest *ci)
{
	struct zestrpc_bulk_desc *desc;
	struct zestion_rpciothr *zri;
	struct zestion_thread *zthr;
	struct l_wait_info lwi;
	struct ciod_wire *cw;
	int i, sum, rc, comms_error;
	u8 *v1;

	zthr = zestion_threadtbl_get();
	zri = &zthr->zthr_rpciothr;
	comms_error = 0;

	cw = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*cw));
	if (cw == NULL) {
		zwarnx("ciod_wire is null");
		rc = errno;
		goto out;
	}
	/*
	 * dump the contents
	 */
	zdbg("ci=%p fd=%d, len=%d, ptygrp_size=%d, ptygrp_pos=%d, ptygrp_genid=%d,"
	     "flags=0%o, crc_buf="LPX64", crc_meta="LPX64", niovs=%d",
	     ci,
	     cw->ciodw_fd,
	     cw->ciodw_len,
	     cw->ciodw_ptygrp_size,
	     cw->ciodw_ptygrp_pos,
	     cw->ciodw_ptygrp_genid,
	     cw->ciodw_flags,
	     cw->ciodw_crc_buf,
	     cw->ciodw_crc_meta,
	     cw->ciodw_iovs.zchunk_niovs);

	/* Sanity check. */
	if (cw->ciodw_len <= 0 || cw->ciodw_len > CIOD_BLOCK_SZ) {
	        zwarnx("len (%u) is invalid (CIOD_BLOCK_SZ=%zu)",
		    cw->ciodw_len, CIOD_BLOCK_SZ);
		rc = EINVAL;
		goto out;
	}

	/* Do a GET of the data, block descriptor, and net descriptor. */
	desc = zestrpc_prep_bulk_exp(req, 1, BULK_GET_SINK, RPCIO_BULK_PORTAL);
	if (desc == NULL) {
		zwarnx("zestrpc_prep_bulk_exp returned a null desc");
		rc = ENOMEM; // errno
		goto out;
	}
	desc->bd_iov[0].iov_base = ci->ciodi_buffer;
	desc->bd_iov[0].iov_len = cw->ciodw_len;
	desc->bd_iov_count = 1;
	desc->bd_nob = cw->ciodw_len;

	/* Check if client was evicted while we were doing i/o before touching
		network */
	if (desc->bd_export->exp_failed)
		rc = ENOTCONN;
	else
		rc = zestrpc_start_bulk_transfer(desc);
	if (rc == 0) {
		lwi = LWI_TIMEOUT_INTERVAL(ZOBD_TIMEOUT / 2,
		    HZ, zrpc_io_bulk_timeout, desc);

		rc = zsvr_wait_event(&desc->bd_waitq,
				     (!zestrpc_bulk_active(desc) ||
				      desc->bd_export->exp_failed),
				     &lwi, NULL);

		LASSERT(rc == 0 || rc == -ETIMEDOUT);
		if (rc == -ETIMEDOUT) {
			zerrorx("timeout on bulk GET");
			zestrpc_abort_bulk(desc);

		} else if (desc->bd_export->exp_failed) {
			zwarnx("eviction on bulk GET");
			rc = -ENOTCONN;
			zestrpc_abort_bulk(desc);

		} else if (!desc->bd_success ||
		    desc->bd_nob_transferred != desc->bd_nob) {
			zerrorx("%s bulk GET %d(%d)",
				desc->bd_success ? "truncated" : "network error on",
				desc->bd_nob_transferred, desc->bd_nob);
			/* XXX should this be a different errno? */
			rc = -ETIMEDOUT;
		}
	} else {
		zinfo("zestrpc bulk get failed: rc %d", rc);
	}
	comms_error = (rc != 0);

	/* count the number of bytes sent, and hold for later... */
	if (rc == 0) {
		v1 = desc->bd_iov[0].iov_base;
		if (v1 == NULL) {
			// don't crash here..
			zerrorx("desc->bd_iov[0].iov_base is NULL");
			rc = ENXIO;
		}

		zinfo("got %u bytes of bulk data across %d IOVs: "
		      "first byte is 0x%x",
		      desc->bd_nob, desc->bd_iov_count, *v1);

		sum = 0;
		for (i=0; i<desc->bd_iov_count; i++)
			sum += desc->bd_iov[i].iov_len;
		if (sum != desc->bd_nob)
			zwarnx("sum (%d) does not match bd_nob (%d)",
			       sum, desc->bd_nob);
		//rc = zestrpc_reply(req);
	}

	/*
	 * and now dispose of it
	 * ??? Is this the right time to do this ???
	 * ??? Don't we need to wait 'til later (or copy it out) before we free it ???
	 */
	zestrpc_free_bulk(desc);

 out:
	if (rc == 0) {
		/* moved this section up a few lines, above zestrpc_free_bulk(desc) */
		memcpy(&ci->ciodi_cs->ciods_data, cw, sizeof(*cw));

		atomic_add(zri->zri_ci->ciodi_cs->ciods_data.ciodw_len,
		    &zri->zri_st_netperf.ist_bytes_intv);
		atomic_add(zri->zri_ci->ciodi_cs->ciods_data.ciodw_len,
		    &rpcIOStats.ist_bytes_intv);
	} else if (!comms_error) {
		/* Only reply if there was no comms problem with bulk */
		req->rq_status = rc;
		zestrpc_error(req);

		atomic_inc(&zri->zri_st_netperf.ist_errors_intv);
		atomic_inc(&rpcIOStats.ist_errors_intv);
	} else {
#if 0
		// For now let's not free the reply state..
		if (req->rq_reply_state != NULL) {
			/* reply out callback would free */
			zestrpc_rs_decref(req->rq_reply_state);
			req->rq_reply_state = NULL;
			req->rq_repmsg      = NULL;
		}
#endif
		CWARN("ignoring bulk IO comm error; id %s - "
			"client will retry\n",
			libcfs_id2str(req->rq_peer));
	}
	return (rc);
}

/*
 * zio_handle_write - process a client RPC WRITE I/O request.
 * @req: client request.
 * Returns: write(2) return value, sends a reply message with
 *	errno or zero on success.
 */
int
zio_handle_write(struct zestrpc_request *req)
{
	struct zestion_rpciothr *zri;
	struct zestion_thread *zthr;
	struct zio_reply_body *repbody;
	struct ciod_wire *cw = NULL;
	struct zlist_head *e;
	struct zeil *zeil;
	int size, rc;

	zthr = zestion_threadtbl_get();
	zri = &zthr->zthr_rpciothr;

	if (zri->zri_ci == NULL) {
		/* Save in case we bail from error to prevent leak. */
		e = zlist_cache_get(&ciodiFreeList, 1);
		zri->zri_ci = zlist_entry(e, struct ciod_ingest,
					  ciodi_zig_entry);
	}
	/*
	 * Not replying the way the client expects is bad
	 */
	size = sizeof(*repbody);
	rc = zest_pack_reply(req, 1, &size, NULL);
	if (rc) {
		zest_assert(rc == -ENOMEM);
		znotify("zest_pack_reply failed");
		/* the client will probably bomb here */
		return rc;
	}

	rc = zio_handle_write_rpc(req, zri->zri_ci);
	if (rc)
		goto done;

	cw = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*cw));
	zest_assert(cw);

	DEBUG_REQ(ZLL_INFO, req, "fd=%d magic="LPX64, 
		  cw->ciodw_fd, cw->ciodw_fdmagic);


#ifndef NETPERF
#ifdef SERVER_CRC_CHECK
	/* Validate checksum of incoming data. */
	if (!crc_valid(cw->ciodw_crc_buf,
	    zri->zri_ci->ciodi_buffer, cw->ciodw_len)) {
		rc = EINVAL;
		goto done;
	}
#endif

	/* Get a validated inode for our fid */
	rc = zeil_lookup(req->rq_export, cw->ciodw_fd, cw->ciodw_fdmagic, &zeil);
	if (rc)
		goto done;

	/* Convert ciod_wire into ciod_ingest and feed it the I/O threads. */
	zri->zri_ci->ciodi_pid = req->rq_export->exp_connection->c_peer.pid;
	zri->zri_ci->ciodi_nid = req->rq_export->exp_connection->c_peer.nid;
	zri->zri_ci->ciodi_inode = zeil->zeil_ino;

	/* Do the rest of the ciod prep work before unleashing this io */
	COPYFID(&zri->zri_ci->ciodi_cs->ciods_fid,
		&zri->zri_ci->ciodi_inode->zinode_fid);

	CIODS_INUSE_SET(zri->zri_ci->ciodi_cs);
	/*
	 * dump the contents
	 */
	ztrace("ci=%p fd=%d, len=%d, ptygrp_size=%d, ptygrp_pos=%d, ptygrp_genid=%d,"
	       "flags=0%o, fdmagic="LPX64" crc_buf="LPX64", crc_meta="LPX64", niovs=%d, ciods_crc "LPX64,
	       zri->zri_ci,
	       cw->ciodw_fd,
	       cw->ciodw_len,
	       cw->ciodw_ptygrp_size,
	       cw->ciodw_ptygrp_pos,
	       cw->ciodw_ptygrp_genid,
	       cw->ciodw_flags,
	       cw->ciodw_fdmagic,
	       cw->ciodw_crc_buf,
	       cw->ciodw_crc_meta,
	       cw->ciodw_iovs.zchunk_niovs,
	       zri->zri_ci->ciodi_cs->ciods_crc);

	/* Place the chunk on the inode's incoming list. */
	rc = ciodi_put(zri->zri_ci, zeil);
	/* Clear out ciod since it is now in use. */
	if (rc == 0)
		zri->zri_ci = NULL;

	/*
	 * release our read lock on the inode after ciodi_put()
	 *  has dirtied it then check rc
	 */
	zinode_decref(zeil->zeil_ino);
	//znotify("zinode %p refcnt %d (after)",
	//	zeil->zeil_ino, atomic_read(&zeil->zeil_ino->zinode_refcnt));

#endif /* NETPERF */

 done:
	repbody = zest_msg_buf(req->rq_repmsg, 0, size);
	zest_assert(repbody);
	/*
	 * If rc != 0 then cw was not assigned.
	 */
	if (cw != NULL) {
		repbody->nbytes = cw->ciodw_len;
		repbody->crc_meta_magic = cw->ciodw_crc_meta;
	}
	/* Grab a new ciodi for the next write(). */
	if (zri->zri_ci == NULL) {
		e = zlist_cache_get(&ciodiFreeList, 1);
		zri->zri_ci = zlist_entry(e, struct ciod_ingest,
					  ciodi_zig_entry);
	}
	return (rc);
}

int
zrpc_io_handler(struct zestrpc_request *req)
{
	int rc = 0;

	ENTRY;

	DEBUG_REQ(ZLL_TRACE, req, " ");

	switch (req->rq_reqmsg->opc) {
	case ZIO_WRITE:
		zdbg("ZIO_WRITE");
		rc = req->rq_status = zio_handle_write(req);
		break;
	case ZIO_CONNECT:
		zdbg("ZIO_CONNECT");
		rc = req->rq_status = zio_handle_connect(req);
		break;
	default:
		zerrorx("Unexpected opcode %d", req->rq_reqmsg->opc);
		req->rq_status = -ENOSYS;
		rc = zestrpc_error(req);
		RETURN(rc);
	}
	zinfo("req->rq_status == %d", req->rq_status);

	target_send_reply_msg(req, rc, 0);

	RETURN(0);
}
