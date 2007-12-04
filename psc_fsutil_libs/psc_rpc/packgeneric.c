/* $Id: pscPackGeneric.c 1868 2007-10-12 18:52:11Z yanovich $ */

#include "psc_util/subsys.h"
#define SUBSYS ZS_RPC

#define _GNU_SOURCE //strnlen

#include <string.h>
#include <stddef.h>

#include "libcfs/libcfs.h"
#include "libcfs/kp30.h"

#include "psc_util/alloc.h"
#include "psc_util/atomic.h"
#include "psc_ds/list.h"
#include "psc_util/lock.h"
#include "psc_rpc/rpc.h"
#include "psc_util/waitq.h"

#define HDR_SIZE(count) \
    size_round(offsetof (struct psc_msg, buflens[(count)]))

int psc_msg_swabbed(struct psc_msg *msg)
{
        return (msg->magic == __swab32(PSCRPC_MSG_MAGIC));
}

static void
psc_init_msg (struct psc_msg *msg, int count, int *lens, char **bufs)
{
        char *ptr;
        int   i;

        msg->magic = PSCRPC_MSG_MAGIC;
        msg->version = PSCRPC_MSG_VERSION;
        msg->bufcount = count;
        for (i = 0; i < count; i++)
                msg->buflens[i] = lens[i];

        if (bufs == NULL)
                return;

        ptr = (char *)msg + HDR_SIZE(count);
        for (i = 0; i < count; i++) {
                char *tmp = bufs[i];
                LOGL(tmp, lens[i], ptr);
        }
}

int psc_pack_request (struct pscrpc_request *req,
                         int count, int *lens, char **bufs)
{
        int reqlen;
        ENTRY;

        reqlen = psc_msg_size (count, lens);
        /* See if we got it from prealloc pool */
        if (req->rq_reqmsg) {
                /* Cannot return error here, that would create
		   infinite loop in pscrpc_prep_req_pool */
                /* In this case pscrpc_prep_req_from_pool sets req->rq_reqlen
		   to maximum size that would fit into this preallocated
		   request */
                LASSERTF(req->rq_reqlen >= reqlen, "req->rq_reqlen %d, "
			 "reqlen %d\n",req->rq_reqlen,
			 reqlen);
                memset(req->rq_reqmsg, 0, reqlen);
        } else {
                ZOBD_ALLOC(req->rq_reqmsg, reqlen);
                if (req->rq_reqmsg == NULL)
                        RETURN(-ENOMEM);
        }
        req->rq_reqlen = reqlen;

	//zinfo("request %p request->rq_reqmsg %p",
	//      req, req->rq_reqmsg);

	psc_init_msg (req->rq_reqmsg, count, lens, bufs);

	//	zinfo("request %p request->rq_reqmsg %p",
	//      req, req->rq_reqmsg);

        RETURN (0);
}


int psc_pack_reply (struct pscrpc_request *req,
                       int count, int *lens, char **bufs)
{
        struct pscrpc_reply_state *rs;
        int                         msg_len;
        int                         size;
        ENTRY;

        LASSERT (req->rq_reply_state == NULL);

        msg_len = psc_msg_size (count, lens);
        size = offsetof (struct pscrpc_reply_state, rs_msg) + msg_len;
        ZOBD_ALLOC (rs, size);
        if (unlikely(rs == NULL)) {
                //rs = lustre_get_emerg_rs(req->rq_rqbd->rqbd_service, size);
                if (!rs)
                        RETURN (-ENOMEM);
        }
        atomic_set(&rs->rs_refcount, 1);        /* 1 ref for rq_reply_state */
        rs->rs_cb_id.cbid_fn  = reply_out_callback;
        rs->rs_cb_id.cbid_arg = rs;
        rs->rs_service = req->rq_rqbd->rqbd_service;
        rs->rs_size = size;
        INIT_PSCLIST_ENTRY(&rs->rs_list_entry);
#if 0
        INIT_PSCLIST_HEAD(&rs->rs_exp_list);
        INIT_PSCLIST_HEAD(&rs->rs_obd_list);
#endif

        req->rq_replen = msg_len;
        req->rq_reply_state = rs;
        req->rq_repmsg = &rs->rs_msg;
        psc_init_msg (&rs->rs_msg, count, lens, bufs);

        //PSCRPC_RS_DEBUG_LRU_ADD(rs);

        RETURN (0);
}

/* This returns the size of the buffer that is required to hold a psc_msg
 * with the given sub-buffer lengths. */
int psc_msg_size(int count, int *lengths)
{
        int size;
        int i;

        size = HDR_SIZE (count);
        for (i = 0; i < count; i++)
                size += size_round(lengths[i]);

        return size;
}

int psc_unpack_msg(struct psc_msg *m, int len)
{
        int   flipped;
        int   required_len;
        int   i;
        ENTRY;

        /* We can provide a slightly better error log, if we check the
         * message magic and version first.  In the future, struct
	 * psc_msg may grow, and we'd like to log a version mismatch,
	 * rather than a short message.
	 *
	 */
        required_len = MAX (offsetof (struct psc_msg, version) +
                            sizeof (m->version),
                            offsetof (struct psc_msg, magic) +
                            sizeof (m->magic));
        if (len < required_len) {
                /* can't even look inside the message */
                CERROR ("msg length %d too small for magic/version check\n",
                        len);
                RETURN (-EINVAL);
        }

        flipped = psc_msg_swabbed(m);
        if (flipped)
                __swab32s (&m->version);
        else if (m->magic != PSCRPC_MSG_MAGIC) {
                CERROR("wrong psc_msg magic %#08x\n", m->magic);
                RETURN (-EINVAL);
        }

#if 0
        if ((m->version & ~LUSTRE_VERSION_MASK) != PSCRPC_MSG_VERSION) {
                CERROR("wrong psc_msg version %#08x\n", m->version);
                RETURN (-EINVAL);
        }
#endif

        /* Now we know the sender speaks my language (but possibly flipped)..*/
        required_len = HDR_SIZE(0);
        if (len < required_len) {
                /* can't even look inside the message */
                CERROR ("message length %d too small for psc_msg\n", len);
                RETURN (-EINVAL);
        }

        if (flipped) {
                __swab32s (&m->type);
                __swab32s (&m->opc);
                __swab64s (&m->last_xid);
                __swab64s (&m->last_committed);
                __swab64s (&m->transno);
                __swab32s (&m->status);
                __swab32s (&m->flags);
                __swab32s (&m->conn_cnt);
                __swab32s (&m->bufcount);
        }

        required_len = HDR_SIZE(m->bufcount);

        if (len < required_len) {
                /* didn't receive all the buffer lengths */
                CERROR ("message length %d too small for %d buflens\n",
                        len, m->bufcount);
                RETURN(-EINVAL);
        }

        for (i = 0; i < (int)m->bufcount; i++) {
                if (flipped)
                        __swab32s (&m->buflens[i]);
                required_len += size_round(m->buflens[i]);
        }

        if (len < required_len) {
                CERROR("len: %d, required_len %d\n", len, required_len);
                CERROR("bufcount: %d\n", m->bufcount);
                for (i = 0; i < (int)m->bufcount; i++)
                        CERROR("buffer %d length %d\n", i, m->buflens[i]);
                RETURN(-EINVAL);
        }

        RETURN(0);
}


/**
 * psc_msg_buflen - return the length of buffer @n in message @m
 * @m - psc_msg (request or reply) to look at
 * @n - message index (base 0)
 *
 * returns zero for non-existent message indices
 */
int psc_msg_buflen(struct psc_msg *m, int n)
{
        if (n >= (int)m->bufcount)
                return 0;

        return m->buflens[n];
}
//EXPORT_SYMBOL(psc_msg_buflen);

void *psc_msg_buf(struct psc_msg *m, int n, int min_size)
{
        int i;
        int offset;
        int buflen;
        int bufcount;

        LASSERT (m != NULL);
        LASSERT (n >= 0);

        bufcount = m->bufcount;
        if (n >= bufcount) {
                CDEBUG(D_INFO, "msg %p buffer[%d] not present (count %d)\n",
                       m, n, bufcount);
                return NULL;
        }

        buflen = m->buflens[n];
        if (buflen < min_size) {
                CERROR("msg %p buffer[%d] size %d too small (required %d)\n",
                       m, n, buflen, min_size);
                return NULL;
        }

        offset = HDR_SIZE(bufcount);
        for (i = 0; i < n; i++)
                offset += size_round(m->buflens[i]);

        return (char *)m + offset;
}


char *psc_msg_string (struct psc_msg *m, int idx, int max_len)
{
        /* max_len == 0 means the string should fill the buffer */
        char *str = psc_msg_buf (m, idx, 0);
        int   slen;
        int   blen;

        if (str == NULL) {
                CERROR ("can't unpack string in msg %p buffer[%d]\n",
			m, idx);
                return (NULL);
        }

        blen = m->buflens[idx];
        slen = strnlen (str, blen);

        if (slen == blen) {                     /* not NULL terminated */
                CERROR ("can't unpack non-NULL terminated string in "
                        "msg %p buffer[%d] len %d\n", m, idx, blen);
                return (NULL);
        }

        if (max_len == 0) {
                if (slen != blen - 1) {
                        CERROR ("can't unpack short string in msg %p "
                                "buffer[%d] len %d: strlen %d\n",
                                m, idx, blen, slen);
                        return (NULL);
                }
        } else if (slen > max_len) {
                CERROR ("can't unpack oversized string in msg %p "
                        "buffer[%d] len %d strlen %d: max %d expected\n",
                        m, idx, blen, slen, max_len);
                return (NULL);
        }

        return (str);
}

#if 0  //Ptl only sm
/* Wrap up the normal fixed length cases */
void *psc_swab_buf(struct psc_msg *msg, int index, int min_size,
		    void *swabber)
{
        void *ptr;

        ptr = psc_msg_buf(msg, index, min_size);
        if (ptr == NULL)
                return NULL;

        if (swabber != NULL && psc_msg_swabbed(msg))
                ((void (*)(void *))swabber)(ptr);

        return ptr;
}

void *psc_swab_reqbuf(struct pscrpc_request *req, int index, int min_size,
                         void *swabber)
{
        LASSERT_REQSWAB(req, index);
        return psc_swab_buf(req->rq_reqmsg, index, min_size, swabber);
}

void *psc_swab_repbuf(struct pscrpc_request *req, int index, int min_size,
                         void *swabber)
{
        LASSERT_REPSWAB(req, index);
        return psc_swab_buf(req->rq_repmsg, index, min_size, swabber);
}

#endif
