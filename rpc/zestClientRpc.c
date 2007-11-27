/* $Id: zestClientRpc.c 2128 2007-11-03 23:54:31Z pauln $ */

#include <sys/param.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "zestClientRpc.h"
#include "zestConfig.h"
#include "zestList.h"
#include "zestLog.h"
#include "zestRpc.h"
#include "zestRpcIO.h"
#include "zestRpcLog.h"
#include "zestRpcMDS.h"
#include "cdefs.h"

lnet_process_id_t	my_id;

struct zestrpc_nbreqset *ioNbReqSet = NULL;

int
zclient_get_server_pid(void)
{
	char *s;

	if ((s = getenv("ZEST_SERVER_PID")) != NULL)
		zwarnx("Ignoring ZEST_SERVER_PID ;%s;, using %d instead",
		       s, ZEST_SVR_PID);
	return (ZEST_SVR_PID);
}

/*
 * RPC library: open a file
 * ofd: file descriptor from client library
 *      - client must make sure that this is unique, until close() is called
 * ofnam: file name
 * oflags: open flags
 * omode: open mode
 */
int zclient_mds_open(int ofd, const char *ofnam, u32 oflags, u32 omode, u64 *fdmagic)
{
        struct zestrpc_import    *mds_import = get_mds_import;
	struct zestrpc_request   *req;
	struct zestrpc_bulk_desc *desc;
	struct zmds_create_body  *body;
	struct zmds_fstat_body   *repbody;

        int size[] = { sizeof(struct zmds_create_body) };
        int rc, len, bufcount = 1;

	len = strlen(ofnam);
	if (len <= 0 || len > PATH_MAX) {
		errno = EINVAL;
		ztrace("invalid len (%d) -- aborting", len);
		return (-1);
	}

	/*
	 * Create the request and associate it with the import
	 */
	req = zestrpc_prep_req(mds_import, ZMDS_VERSION,
                              ZMDS_OPEN, bufcount, size, NULL);
	if (req == NULL){
//		errno = ENOMEM;
	        zerror("NULL request -- aborting");
		return (-1);
	}

	/*
	 * malloc the msg body, cast it, and assign its members
	 */
        body = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	if (body == NULL) {
		zerror("NULL msgbuf");
		return (-1);
	}	
	body->flen  = len;
	body->uid   = getuid();
	body->gid   = getgid();
	body->flags = oflags;
	body->mode  = omode;
	body->fd    = ofd;
	/*
	 * setup the memory and structures for a 'GET'
	 */
	desc = zestrpc_prep_bulk_imp(req, 1, BULK_GET_SOURCE,
				     RPCMDS_BULK_PORTAL);
	if (desc == NULL) {
		zestrpc_free_req(req);
		//errno = ENOMEM;
		ztrace("NULL descriptor -- aborting");
		return (-1);
	}

	desc->bd_iov[0].iov_base = (char *)ofnam;
        desc->bd_iov[0].iov_len  = body->flen;
        desc->bd_iov_count       = 1;
	desc->bd_nob             = body->flen;
	/*
	 * predetermine the reply length, the reply buffer is
	 *  posted within zestrpc_queue_wait()
	 */
        req->rq_replen = zest_msg_size(1, size);
	/*
	 * send the request and block on its completion
	 */
	rc = zestrpc_queue_wait(req);
	if (rc)
		goto out_req;
	/*
	 * point to the reply body
	 */
	repbody = zest_msg_buf(req->rq_repmsg, 0, sizeof(*repbody));
        if (repbody == NULL) {
                zerrorx("reply body is null");
		rc = EPROTO;
		goto out_req;
        }

	DEBUG_REQ(ZLL_INFO, req, LPX64" repbody fdmagic", 
		  repbody->zfb_clientfdmagic);

	zest_assert(repbody->zfb_clientfdmagic);
	*fdmagic = repbody->zfb_clientfdmagic;

 out_req:
	// XXX free desc
	zestrpc_req_finished(req);
	if (rc)
		errno = rc;
	return (rc ? -1 : 0);
}

int
zclient_mds_close(int fd, u64 fdmagic)
{
	struct zestrpc_import *mds_import;
	struct zestrpc_request *req;
	struct zmds_close_body *body;
	int size, rc;

	ztrace("closing FD %d", fd);

	/* Create the request and associate it with the import.  */
	mds_import = get_mds_import;
	size = sizeof(*body);
	req = zestrpc_prep_req(mds_import, ZMDS_VERSION, ZMDS_CLOSE, 1,
			       &size, NULL);
	if (req == NULL)
		return (-ENOMEM);

	/* Allocate the msg body and assign its members. */
	body = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	body->clientfd = fd;
	body->fdmagic = fdmagic;

	size = 0;
	req->rq_replen = zest_msg_size(1, &size);

	/* Send the request and block on its completion */
	rc = zestrpc_queue_wait(req);

	zdbg("return value is %d", rc);

	zestrpc_req_finished(req);
	return (rc);
}

int
zclient_mds_fstat(int fd, struct stat *stb)
{
	struct zestrpc_import *mds_import;
	struct zmds_fstat_repbody *zfrb;
	struct zestrpc_request *req;
	struct zmds_fstat_body *zfb;
	int size, rc;
	
	/* Create the request and associate it with the import.  */
	mds_import = get_mds_import;
	size = sizeof(*zfb);
	req = zestrpc_prep_req(mds_import, ZMDS_VERSION, ZMDS_FSTAT, 1,
			       &size, NULL);
	if (req == NULL)
		return (-ENOMEM);
	/* Assign msg body members. */
	zfb = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*zfb));
	zfb->zfb_clientfd = fd;
	size = sizeof(*zfrb);
	req->rq_replen = zest_msg_size(1, &size);
	
	/* Send the request and block on its completion */
	rc = zestrpc_queue_wait(req);
	if (rc)
		goto out_req;
	zfrb = zest_msg_buf(req->rq_repmsg, 0, sizeof(*zfrb));
        if (zfrb == NULL) {
                zerrorx("fstat_repbody is NULL");
		rc = EPROTO;
		goto out_req;
	}

	/* extract the reply body into the client's stat buffer */
	stb->st_size = zfrb->zfrb_size;

 out_req:
	// XXX free desc
	zestrpc_req_finished(req);
	if (rc)
		errno = rc;
	return (rc ? -1 : 0);
}


int zclient_mds_connect(lnet_nid_t server)
{
	struct zestrpc_request *req;
	struct zestrpc_import *mds_import;
	lnet_process_id_t      server_id = {server, 0};

        int size[] = { sizeof(struct zmds_connect_body) };
	struct zmds_connect_body *body;
        int rc, bufcount = 1;

        ENTRY;

	if (LNetGetId( 1, &my_id))
                zfatalx("LNetGetId() failed");

	mds_import = get_mds_import;

	mds_import->imp_connection = zestrpc_get_connection(server_id,
							    my_id.nid, NULL);
	/*
	 * We don't know the server's pid just yet so allow any pid
	 *  to reply.
	 */
	mds_import->imp_connection->c_peer.pid = zclient_get_server_pid();

	zdbg("mds_import addr %p %p, server pid=%u",
	     mds_import, mds_import->imp_client, 
	     mds_import->imp_connection->c_peer.pid);

	req = zestrpc_prep_req(mds_import, ZMDS_VERSION,
                              ZMDS_CONNECT, bufcount, size, NULL);

	if (req == NULL)
                RETURN(-ENOMEM);

	body = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
        if (body == NULL) {
                zwarnx("connect_body is null");
		return -1;
        }

	body->fsversion = ZMDS_VERSION;
	body->fsmagic   = ZMDS_CONNECT_MAGIC;

	size[0] = sizeof(struct zmds_connect_body);
        req->rq_replen = zest_msg_size(1, size);

	rc = zestrpc_queue_wait(req);
	if (rc) {
		errno = -rc;
		zfatal("MDS Connect Failed");
		return rc;
	}
	/*
	 * The pid was handed back via reply_in_callback
	 */
	mds_import->imp_connection->c_peer.pid = req->rq_peer.pid;
        zdbg("rq_peer ;%s;", libcfs_id2str(req->rq_peer));
	/*
	 * Mark the import as fully initialized
	 */
	mds_import->imp_state = ZEST_IMP_FULL;

	return 0;
}

int
zclient_io_connect(lnet_nid_t server)
{
	struct zestrpc_import *io_import;
	struct zestrpc_request *req;
	struct zio_connect_body *zicb;
	lnet_process_id_t      server_id = {server, 0};
	int size, rc, bufcount = 1;

	ENTRY;

	if (LNetGetId( 1, &my_id))
		zfatalx("LNetGetId() failed");

	io_import = get_io_import;
	io_import->imp_connection = zestrpc_get_connection(server_id,
	    my_id.nid, NULL);
	/*
	 * We don't know the server's PID just yet so allow any PID
	 *  to reply.
	 */
	io_import->imp_connection->c_peer.pid = zclient_get_server_pid();
	zdbg("io_import addr %p %p", io_import, io_import->imp_client);

	size = sizeof(*zicb);
	req = zestrpc_prep_req(io_import, ZIO_VERSION,
	    ZIO_CONNECT, bufcount, &size, NULL);

	if (req == NULL)
		RETURN(-ENOMEM);

	zicb = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*zicb));
	if (zicb == NULL) {
		zwarnx("io_connect body is null");
		return -1;
	}
	zicb->zicb_version = ZIO_VERSION;
	zicb->zicb_magic   = ZIO_CONNECT_MAGIC;

	size = sizeof(*zicb);
	req->rq_replen = zest_msg_size(1, &size);

	rc = zestrpc_queue_wait(req);
	if (rc) {
		errno = -rc;
		zfatal("io_connect Failed");
		return rc;
	}
	/*
	 * The PID was handed back via reply_in_callback
	 */
	io_import->imp_connection->c_peer.pid = req->rq_peer.pid;
	zdbg("rq_peer ;%s;", libcfs_id2str(req->rq_peer));
	/*
	 * Mark the import as fully initialized
	 */
	io_import->imp_state = ZEST_IMP_FULL;

	return 0;
}

/** 
 * zest_client_nbcallback - call me when an async operation has completed
 *
 */
int
zest_client_nbcallback(struct zestrpc_request    *req,
		       struct zestrpc_async_args *cb_args) 
{
	zest_stream_buffer_t *zsb;
	/*
	 * Catch bad status here, we can't proceed if a 
	 *  nb buffer did not send properly.
	 */
	zsb = cb_args->pointer_arg[ZSB_CB_POINTER_SLOT];
	zest_assert(zsb);

	DEBUG_REQ(ZLL_INFO, req, "processing via nbcallback zsb %p", zsb);

	if (req->rq_status) {
		DEBUG_REQ(ZLL_ERROR, req, "ouch, non-zero rq_status");
                zfatalx("IO Req could not be completed, sorry");
	}
	zest_assert(zsb->zsb_zcf);

	spinlock(&zsb->zsb_zcf->zcf_lock);
	zlist_del_init(&zsb->zsb_ent);
	freelock(&zsb->zsb_zcf->zcf_lock);
	/*
	 * got what we need to free the buffer
	 */
	return(zest_buffer_free(zsb));
}

int 
zest_io_interpret_set(struct zestrpc_request_set *set, 
		      void *arg, int status)
{
	struct zestrpc_request *req;
	struct zlist_head      *tmp;
	int rc = 0;
	/*
	 * zestrpc_set_wait() already does this for us but it
	 *  doesn't abort.
	 */
	zlist_for_each(tmp, &set->set_requests) {
		req = zlist_entry(tmp, struct zestrpc_request, rq_set_chain);
		
		LASSERT(req->rq_phase == ZRQ_PHASE_COMPLETE);
		if (req->rq_status != 0) {
			/* sanity check */
			zest_assert(status);

			rc = req->rq_status;
			DEBUG_REQ(ZLL_ERROR, req, 
				  "Found failed request (set %p)", set);
		}
	}	
	/*
	 * This is harsh but it's ok for now
	 */
	if (rc)
		zfatalx("Some IO Reqs could not be completed, sorry");
	
	return rc;
}

/**
 * zest_io_req_interpret_reply - reply handler meant to be used directly 
 *   or as a callback.
 *
 */
int 
zest_io_req_interpret_reply(struct zestrpc_request *req, 
			    void *arg, int status)
{
	struct zestrpc_async_args *rq_async_args;
	struct zio_reply_body     *repbody;
	struct ciod_wire          *reqbody;
	
	rq_async_args = (struct zestrpc_async_args *)arg;
	/*
	 * Map the request and reply structure
	 */
	reqbody = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*reqbody));
	repbody = zest_msg_buf(req->rq_repmsg, 0, sizeof(*repbody));

	zest_assert(reqbody);

	if (repbody == NULL) {
		zerrorx("reply body is null");
		return(-EPROTO);
	}
	
	if (repbody->nbytes != reqbody->ciodw_len) { 
		DEBUG_REQ(ZLL_ERROR, req, "nbytes differ req %u, rep %u", 
			  reqbody->ciodw_len, repbody->nbytes);
		return(-EIO);
	}

	if (repbody->crc_meta_magic != reqbody->ciodw_crc_meta) {
                DEBUG_REQ(ZLL_ERROR, req, "crc magic differs req %"
			  ZLPX64", rep %"ZLPX64,
                          reqbody->ciodw_crc_meta, repbody->crc_meta_magic);
		return(-EIO);
        }

	if (req->rq_status)
		DEBUG_REQ(ZLL_WARN, req, "ignoring non-zero rq_status");

	if (status) {
		DEBUG_REQ(ZLL_ERROR, req, "non-zero status status %d, "
			  "rq_status %d", status, req->rq_status);
		return(status);
	}

	DEBUG_REQ(ZLL_INFO, req, "IO Write OK nbytes=%u", 
		  repbody->nbytes);

	
	return(zest_client_nbcallback(req, rq_async_args));
}

/**
 * _zclient_io_request_create - private function for converting
 *   zest client-side structures into a zest write rpc request.
 * @ciodw: client io wire struct
 * @bulk_iov: iov array
 * @niov: num iovs
 * Notes:  bulk_iov is mapped onto bulk desc here
 */
static struct zestrpc_request *
_zclient_io_request_create(zest_stream_buffer_t *zsb) 
{
	struct zestrpc_request   *req;
	struct zestrpc_import    *io_import;
	struct zestrpc_bulk_desc *desc;
	//struct ciod_wire         *body;

	int  i;
        int  size[] = {sizeof(struct ciod_wire)};
	char *args[] = {(char *)&zsb->zsb_ciodw};
	int nob;

	ENTRY;

	io_import = get_io_import;
	req       = zestrpc_prep_req(io_import, ZIO_VERSION, ZIO_WRITE, 1,
				     size, args);

	if (req == NULL) {
		//errno = ENOMEM;
	        ztrace("NULL request -- aborting");
		RETURN (NULL);
	}
	/*
	 * Setup state for callbacks
	 */
	req->rq_interpret_reply = (void *)zest_io_req_interpret_reply;
	req->rq_async_args.pointer_arg[ZSB_CB_POINTER_SLOT] = zsb;

	nob = 0;
	for (i = 0; i < zsb->zsb_niov; i++)
		nob += zsb->zsb_iov[i].iov_len;

	if (nob <= 0){
	        ztrace("no data in bulk_iov (niov=%hd, nob=%d)-- aborting", 
		       zsb->zsb_niov, nob);
		RETURN (NULL);
	}

	/* Setup memory for 'GET'.  */
	//body = zest_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	//memcpy(body, &zsb->zsb_ciodw, sizeof(*body));
	//body->ciodw_flags |= CIODF_END_GROUP;

	/* Tell server he can 'GET' our data now. */
	desc = zestrpc_prep_bulk_imp(req, zsb->zsb_niov, BULK_GET_SOURCE,
				     RPCIO_BULK_PORTAL);
	if (desc == NULL) {
		zestrpc_free_req(req);
		//errno = ENOMEM;
		ztrace("NULL descriptor -- aborting");
		RETURN (NULL);
	}

	desc->bd_iov_count = zsb->zsb_niov;
	desc->bd_nob       = nob;
	for (i = 0; i < zsb->zsb_niov; i++) {
		desc->bd_iov[i].iov_base = zsb->zsb_iov[i].iov_base;
		desc->bd_iov[i].iov_len  = zsb->zsb_iov[i].iov_len;
	}

	zdbg("bd_iov_count=%d, bd_nob=%d", 
	     desc->bd_iov_count, desc->bd_nob);
	/*
	 * Predetermine reply length, which will
	 * later be posted in zestrpc_queue_wait().
	 */
	size[0] = sizeof(struct zio_reply_body);
	req->rq_replen = zest_msg_size(1, size);
	
	RETURN(req);
}

/**
 * zclient_io_request_create - external wrapper for _zclient_io_request_create
 */
struct zestrpc_request *
zclient_io_request_create(zest_stream_buffer_t *zsb) 
{
	return (_zclient_io_request_create(zsb));
}

/**
 * zclient_io_nbwrite - asynchronous, single-block write
 *                    RPC library: write bulk data
 * ciodw: pointer to wire protocol struct
 *        - assume that it is packed in the calling functions
 * bulk_iov: bulk data, packed into IOVEC array
 * niov: length of bulk_iov array
 */
ssize_t
zclient_io_nbwrite(zest_stream_buffer_t *zsb)
{
	struct zestrpc_request *req;	
	int nb_buffers_reaped = 0;
	/*
	 * Check the non-blocking request structure for initialization, 
	 *  this could be done on-the-fly if we allocated a lock.
	 */
	zest_assert(ioNbReqSet);

	req = _zclient_io_request_create(zsb);
	if (req == NULL)
		return -1;

	(void)nbreqset_add(ioNbReqSet, req);
	/*
	 * Set mgmt, query the set for completed ops
	 */	
	nb_buffers_reaped = nbrequest_reap(ioNbReqSet); 
	if (nb_buffers_reaped == -EAGAIN) 		
		zinfo("nbrequest_reap() reaped no buffers");
	else 
		zinfo("nbrequest_reap() found %d completed buffers",
                      nb_buffers_reaped);
	/*
	 * No couth here though nbreqset_add() does abort
	 *  if needed
	 */
	return 0;
}
/**
 * zclient_io_write - synchronous, single-block write
 *                    RPC library: write bulk data
 * ciodw: pointer to wire protocol struct
 *        - assume that it is packed in the calling functions
 * bulk_iov: bulk data, packed into IOVEC array
 * niov: length of bulk_iov array
 */
ssize_t
zclient_io_write(zest_stream_buffer_t *zsb)
{
	struct zestrpc_request *req;

	req = _zclient_io_request_create(zsb);
	if (req == NULL)
		return -1;
	/*
	 * Send it and wait for a reply
	 */
	return(zest_io_req_interpret_reply(req, NULL, 
					   zestrpc_queue_wait(req)) );
}

struct zclient_service *
zclient_service_create(__unusedx lnet_nid_t server,
		       u32 request_portal,
		       u32 reply_portal,
		       service_connect connect_fn)
{
	struct zclient_service *csvc;

	csvc = ZALLOC(sizeof(*csvc));

	INIT_ZLIST_HEAD(&csvc->csvc_old_imports);
	LOCK_INIT(&csvc->csvc_lock);

	csvc->csvc_failed = 0;
	csvc->csvc_initialized = 0;

	csvc->csvc_import = new_import();
	if (!csvc->csvc_import) {
		zfatalx("Failed to allocate new import");
	}

	csvc->csvc_import->imp_client = ZALLOC(sizeof(struct zestrpc_client));
	csvc->csvc_import->imp_client->cli_request_portal = request_portal;
	csvc->csvc_import->imp_client->cli_reply_portal   = reply_portal;

	csvc->csvc_import->imp_max_retries = 2;

	csvc->csvc_connect = connect_fn;

	zdbg("created service at %p import %p imp_client %p portals %u:%u",
	     csvc, csvc->csvc_import, csvc->csvc_import->imp_client,
	     csvc->csvc_import->imp_client->cli_request_portal,
	     csvc->csvc_import->imp_client->cli_reply_portal);
	return csvc;
}


int zclient_services_init(void)
{
	int           rc;
	char         *zclient_server_name;
	lnet_nid_t    zclient_server_nid;
	struct        zclient_service *csvc;
	extern struct zclient_service *zclient_service_dispatcher[NUM_ZEST_CLIENT_SVCS];

#ifndef XT3CLIENT_SYS
	/*
	 * If using the XT3 system libs (aka liblustre), don't init twice
	 */
	rc = zestrpc_init_portals(ZEST_CLIENT);
	if (rc)
		zfatal("Failed to intialize portals rpc");
#endif

	/*
	 * First handle ZEST_CLIENT_MDS_SERVICE
	 */
	zclient_server_name = getenv("ZEST_SERVER_NID");

	if (zclient_server_name) {
		zclient_server_nid = libcfs_str2nid(zclient_server_name);
		if (zclient_server_nid == LNET_NID_ANY)
			zfatalx("ZEST_SERVER_NID ;%s; is invalid",
				zclient_server_name);
		zdbg("zclient_server_name is ;%s; nid %"ZLPX64,
		     zclient_server_name, zclient_server_nid);
	} else
		zfatalx("Please export ZEST_SERVER_NID");

	csvc = zclient_service_create(zclient_server_nid,
				      RPCMDS_REQUEST_PORTAL,
				      RPCMDS_REPLY_PORTAL,
				      zclient_mds_connect);

	zclient_service_dispatcher[ZEST_CLIENT_MDS_SERVICE] = csvc;

	if (zclient_mds_connect(zclient_server_nid))
		zerror("Failed to connect to ;%s;", zclient_server_name);

	/*
	 * Second handle ZEST_CLIENT_IO_SERVICE
	 */
	csvc = zclient_service_create(zclient_server_nid,
				      RPCIO_REQUEST_PORTAL,
				      RPCIO_REPLY_PORTAL,
				      zclient_io_connect);

	zclient_service_dispatcher[ZEST_CLIENT_IO_SERVICE] = csvc;
	if (zclient_io_connect(zclient_server_nid))
		zerror("Failed to connect to ;%s;", zclient_server_name);
	/*
	 * Init nb_req manager for single-block, non-blocking requests
	 *   TODO: create a callback handler
	 */
	ioNbReqSet = nbreqset_init(zest_io_interpret_set, 
				   zest_client_nbcallback);
	zest_assert(ioNbReqSet);

	return (0);
}
