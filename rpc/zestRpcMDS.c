/* $Id: zestRpcMDS.c 2137 2007-11-05 18:41:27Z yanovich $ */

#include "subsys.h"
#define ZSUBSYS ZS_RPC

#include <errno.h>

#include "zestAssert.h"
#include "zestFileOpsRpc.h"
#include "zestLog.h"
#include "zestRpc.h"
#include "zestRpcLog.h"
#include "zestRpcMDS.h"
#include "zestTypes.h"

/**
 * zmds_handle_connect - simple function for handling a connection request
 * @req: the request as sent by the client
 */
static int zmds_handle_connect(struct zestrpc_request *req)
{
	int rc;
	int size = sizeof(struct zmds_connect_body);
	struct zmds_connect_body  *body, *repbody;

	body = zest_msg_buf(req->rq_reqmsg, 0, size);
	if (body == NULL) {
		zwarnx("connect_body is null");
		rc = -1;
		goto fail;
	}
	znotify("fsmagic %"ZLPX64" version %u",
		body->fsmagic, body->fsversion);


	rc = zest_pack_reply(req, 1, &size, NULL);
	if (rc) {
		zest_assert(rc == -ENOMEM);
		zerror("zest_pack_reply failed");
		goto fail;
	}

	repbody = zest_msg_buf(req->rq_repmsg, 0, size);
	if (repbody == NULL) {
		zwarnx("connect_repbody is null");
		rc = -1;
		goto fail;
	}

	if (body->fsmagic   == ZMDS_CONNECT_MAGIC &&
	    body->fsversion == ZMDS_VERSION)
		/*
		 * Return the body back to the client
		 */
		memcpy(repbody, body, size);

	else {
		repbody->fsmagic = 0;
		rc = -1;
		goto fail;
	}

	znotify("Connect request from %"ZLPX64":%u",
		req->rq_peer.nid, req->rq_peer.pid);

	return 0;
 fail:
	znotify("Failed connect request from %"ZLPX64":%u",
		req->rq_peer.nid, req->rq_peer.pid);

	return rc;
}

int zrpc_mds_handler(struct zestrpc_request *req)
{
	int rc = 0;

	switch (req->rq_reqmsg->opc) {

	case ZMDS_CLOSE:
		zdbg("ZMDS_CLOSE");
		req->rq_status = zmds_file_close(req);
		break;

	case ZMDS_CONNECT:
		zdbg("ZMDS_CONNECT");
		req->rq_status = zmds_handle_connect(req);
		break;

	case ZMDS_CREAT:
	case ZMDS_OPEN:
		zdbg("ZMDS_OPEN");
		req->rq_status = zmds_file_open(req);
		break;

	case ZMDS_MKDIR:
		zdbg("ZMDS_MKDIR");
		break;

	case ZMDS_FSTAT:
		zdbg("ZMDS_FSTAT");
		req->rq_status = zmds_file_fstat(req);
		break;

	case ZMDS_TRUNC:
		zdbg("ZMDS_TRUNC");
		break;

	default:
		zerrorx("Unexpected opcode %d", req->rq_reqmsg->opc);
		req->rq_status = -ENOSYS;
		rc = zestrpc_error(req);
		RETURN(rc);
	}
	zinfo("req->rq_status == %d", req->rq_status);

	target_send_reply_msg (req, rc, 0);

	return 0;
}
