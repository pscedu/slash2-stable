#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

#include "mount_slash.h"
#include "psc_rpc/rpc.h"
#include "psc_util/slash_appthread.h"

#define MDS_NTHREADS  8
#define MDS_NBUFS     1024
#define MDS_BUFSZ     256
#define MDS_REPSZ     128
#define MDS_REQPORTAL RPCMDS_REQ_PORTAL
#define MDS_REPPORTAL RPCMDS_REP_PORTAL
#define MDS_SVCNAME   "slash_mds_svc"

int 
slmds_connect(struct pscrpc_request *req)
{
	int rc;
	int sz = sizeof(struct slashrpc_connect_req);
        struct slashrpc_connect_req *body, *repbody;

	body = psc_msg_buf(req->rq_reqmsg, 0, size);
        if (body == NULL) {
                psc_warnx("connect_body is null");
                rc = -ENOMSG;
                goto fail;
        }
        psc_notify("magic %"ZLPX64" version %u",
		   body->fsmagic, body->fsversion);

        if (body->magic   != SMDS_CONNECT_MAGIC ||
            body->version != SMDS_VERSION) {
		rc = -EINVAL;
		goto fail;
	}				
        rc = psc_pack_reply(req, 1, &size, NULL);
        if (rc) {
                psc_assert(rc == -ENOMEM);
                psc_error("psc_pack_reply failed");
                goto fail;
        }
        repbody = psc_msg_buf(req->rq_repmsg, 0, size);
	/* Malloc was done in psc_pack_reply() */
        psc_assert(repbody);

	repbody->magic  = SMDS_CONNECT_MAGIC;
	repbody->version = SMDS_VERSION;
	
        psc_notify("Connect request from %"ZLPX64":%u",
		   req->rq_peer.nid, req->rq_peer.pid);

        return (0);
 fail:
        psc_notify("Failed connect request from %"ZLPX64":%u",
		   req->rq_peer.nid, req->rq_peer.pid);
        return (rc);
}

int
slmds_access(struct pscrpc_request *req)
{
        int rc;
        struct slashrpc_access_req *body;

	body = psc_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	if (!body)
		return (-EPROTO);
	
	rc = access(body->path, mask);
	if (rc)
		return (-errno);
	return (0);
}

int
slmds_chmod(struct pscrpc_request *req, int which_chmod)
{
        int rc;
        struct slashrpc_chmod_req *body;

	body = psc_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	if (!body)
		return (-EPROTO);
	
	if (which_chmod == SYS_fchmod)
		if (fid_makepath(&body->fid, body->path))
			return (-EINVAL);

	rc = chmod(body->path, mask); 
	if (rc)
		return (-errno);

	return (0);
}

/**
 *
 * Notes:  fchown of a directory cannot be supported in this version since
 *          there is no immutable namespace for directories.
 */
int
slmds_chown(struct pscrpc_request *req, int which_chown)
{
        int rc;
        struct slashrpc_chown_req *body;

	body = psc_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
	if (!body)
		return (-EPROTO);
	
	if (which_chown == SYS_fchown)
		if (fid_makepath(&body->fid, body->path))
			return (-EINVAL);

	rc = chown(body->path, mask); 
	if (rc)
		return (-errno);

	return (0);
}

int
slmds_open(struct pscrpc_request *req)
{
	int rc;
	struct slashrpc_open_req *body;

	body = psc_msg_buf(req->rq_reqmsg, 0, sizeof(*body));
        if (!body)
                return (-EPROTO);

	if (mode)
		rc = open(body->path, body->flags, body->mode);
	else
		rc = open(body->path, body->flags);
	if (rc)
		return (-errno);

	return (0);
}

int 
slmds_svc_handler(struct pscrpc_request *req)
{
	struct slashrpc_cred *cred;
        int rc = 0;
	uid_t myuid;
        gid_t mygid;       
       
        ENTRY;
        DEBUG_REQ(PLL_TRACE, req, "new req");	
	/*
	 * Pop the credentials from the msg stack
	 */
	cred = psc_msg_buf(req->rq_reqmsg, 0, sizeof(*cred));
	if (!cred) {
		psc_warnx("Rpc credentials null");
		req->rq_status = -EPROTO;
                rc = pscrpc_error(req);
                RETURN(rc);
	}
	/*
	 * Set fs credentials 
	 */
	myuid = getuid();
        mygid = getgid();
	if (setfsuid(cred->sc_uid) != (int)myuid) {
                psc_error("invalid uid U%u:G%u", uid, gid);
		rc = -1;
                goto done;
        }

        if (setfsgid(cred->sc_gid) != (int)mygid) {
                psc_error("invalid gid U%u:G%u", uid, gid);
		rc = -1;
                goto done;
        }

	switch (req->rq_reqmsg->opc) {
	case SRMT_CONNECT:
		rc = slmds_connect(req);
		break;
	case SRMT_ACCESS:
		rc = slmds_access(req);
		break;
	case SRMT_CHMOD:
		rc = slmds_chmod(req, SYS_chmod);
		break;
	case SRMT_FCHMOD:
		rc = slmds_chmod(req, SYS_fchmod);
		break;
	case SRMT_CHOWN:
		rc = slmds_chown(req);
		break;
	case SRMT_CREATE:
	case SRMT_OPEN:
		rc = slmds_open(req);
		break;
	case SRMT_GETATTR:
		rc = slmds_getattr(req);
		break;
	case SRMT_LINK:
		rc = slmds_link(req);
		break;
	case SRMT_MKDIR:
		rc = slmds_mkdir(req);
		break;
	case SRMT_RELEASE:
		rc = -EOPNOTSUPP;
		break;
	case SRMT_RENAME:
		rc = slmds_rename(req);
		break;
	case SRMT_RMDIR:
		rc = slmds_rmdir(req);
		break;
	case SRMT_SYMLINK:
		rc = slmds_symlink(req);
		break;
	case SRMT_TRUNCATE:
		rc = slmds_truncate(req, SYS_truncate);
		break;
	case SRMT_FTRUNCATE:
		rc = slmds_truncate(req, SYS_ftruncate);
		break;
	case SRMT_UNLINK:
		rc = slmds_unlink(req);
		break;
	case SRMT_UTIMES:
		rc = slmds_utimes(req);
		break;
	default:
		psc_errorx("Unexpected opcode %d", req->rq_reqmsg->opc);
		req->rq_status = -ENOSYS;
		rc = pscrpc_error(req);
		goto done;
	}	
	psc_info("req->rq_status == %d", req->rq_status);	
	target_send_reply_msg (req, rc, 0);
	
 done:
	if (setfsuid(myuid) == -1)
                psc_fatal("setfsuid %d", myuid);
        if (setfsgid(mygid) == -1)
                psc_fatal("setfsgid %d", mygid);

	RETURN(rc);	
}



/**
 * slmds_init - start up the mds threads via pscrpc_thread_spawn() 
 */
void 
slmds_init(void)
{
	pscrpc_svc_handle_t *svh = PSCALLOC(sizeof(*svc));
	
	svh->svh_nbufs      = MDS_NBUFS;
	svh->svh_bufsz      = MDS_BUFSZ;
	svh->svh_reqsz      = MDS_BUFSZ;
	svh->svh_repsz      = MDS_REPSZ;
	svh->svh_req_portal = MDS_REQPORTAL;
	svh->svh_rep_portal = MDS_REPPORTAL;
	svh->svh_type       = SLASH_MDSTHR;
	svh->svh_nthreads   = MDS_NTHREADS;
	svh->svh_handler    = slmds_svc_handler;

	strncpy(svh->svh_svn_name, MDS_SVCNAME, RPC_SVC_NAMEMAX);
	
	pscrpc_thread_spawn(svh);
}
