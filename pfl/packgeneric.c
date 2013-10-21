/* $Id$ */

/*
 *  Copyright (c) 2002, 2003 Cluster File Systems, Inc.
 *
 *   This file is part of the Lustre file system, http://www.lustre.org
 *   Lustre is a trademark of Cluster File Systems, Inc.
 *
 *   You may have signed or agreed to another license before downloading
 *   this software.  If so, you are bound by the terms and conditions
 *   of that agreement, and the following does not apply to you.  See the
 *   LICENSE file included with this distribution for more information.
 *
 *   If you did not agree to a different license, then this copy of Lustre
 *   is open source software; you can redistribute it and/or modify it
 *   under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   In either case, Lustre is distributed in the hope that it will be
 *   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   license text for more details.
 *
 */

#define PSC_SUBSYS PSS_RPC

#include <string.h>
#include <stddef.h>

#include "libcfs/libcfs.h"
#include "libcfs/kp30.h"

#include "pfl/str.h"
#include "pfl/list.h"
#include "pfl/rpc.h"
#include "pfl/alloc.h"
#include "pfl/atomic.h"
#include "pfl/lock.h"
#include "pfl/waitq.h"

#define HDR_SIZE(count) \
    size_round(offsetof(struct pscrpc_msg, buflens[(count)]))

int
pscrpc_msg_swabbed(struct pscrpc_msg *msg)
{
	return (msg->magic == __swab32(PSCRPC_MSG_MAGIC));
}

static void
pscrpc_init_msg(struct pscrpc_msg *msg, int count, const int *lens,
    char **bufs)
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

int
pscrpc_pack_request(struct pscrpc_request *req, int count, int *lens,
    char **bufs)
{
	int reqlen;

	reqlen = pscrpc_msg_size(count, lens);
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
		PSCRPC_OBD_ALLOC(req->rq_reqmsg, reqlen);
		if (req->rq_reqmsg == NULL)
			return (-ENOMEM);
	}
	req->rq_reqlen = reqlen;

	//pscinfo("request %p request->rq_reqmsg %p",
	//      req, req->rq_reqmsg);

	pscrpc_init_msg(req->rq_reqmsg, count, lens, bufs);

	//	pscinfo("request %p request->rq_reqmsg %p",
	//      req, req->rq_reqmsg);

	return (0);
}

int
pscrpc_pack_reply(struct pscrpc_request *req, int count,
    const int *lens, char **bufs)
{
	struct pscrpc_reply_state *rs;
	int                         msg_len;
	int                         size;

	LASSERT(req->rq_reply_state == NULL);

	msg_len = pscrpc_msg_size(count, lens);
	size = offsetof(struct pscrpc_reply_state, rs_msg) + msg_len;
	PSCRPC_OBD_ALLOC(rs, size);
	if (unlikely(rs == NULL)) {
		//rs = lustre_get_emerg_rs(req->rq_rqbd->rqbd_service, size);
		if (!rs)
			return (-ENOMEM);
	}
	atomic_set(&rs->rs_refcount, 1);        /* 1 ref for rq_reply_state */
	rs->rs_cb_id.cbid_fn  = pscrpc_reply_out_callback;
	rs->rs_cb_id.cbid_arg = rs;
	rs->rs_service = req->rq_rqbd->rqbd_service;
	rs->rs_size = size;
	INIT_PSC_LISTENTRY(&rs->rs_list_entry);
#if 0
	INIT_PSCLIST_HEAD(&rs->rs_exp_list);
	INIT_PSCLIST_HEAD(&rs->rs_obd_list);
#endif

	req->rq_replen = msg_len;
	req->rq_reply_state = rs;
	req->rq_repmsg = &rs->rs_msg;
	pscrpc_init_msg(&rs->rs_msg, count, lens, bufs);

	//PSCRPC_RS_DEBUG_LRU_ADD(rs);

	return (0);
}

/*
 * pscrpc_msg_size -  Calculate the size of the buffer that is required
 *	to hold a pscrpc_msg with the given sub-buffer lengths.
 */
int
pscrpc_msg_size(int count, const int *lengths)
{
	int size;
	int i;

	size = HDR_SIZE(count);
	for (i = 0; i < count; i++)
		size += size_round(lengths[i]);

	return size;
}

int
pscrpc_unpack_msg(struct pscrpc_msg *m, int len)
{
	int   flipped;
	int   required_len;
	int   i;

	/* We can provide a slightly better error log, if we check the
	 * message magic and version first.  In the future, struct
	 * pscrpc_msg may grow, and we'd like to log a version mismatch,
	 * rather than a short message.
	 *
	 */
	required_len = MAX(offsetof(struct pscrpc_msg, version) +
			    sizeof(m->version),
			    offsetof(struct pscrpc_msg, magic) +
			    sizeof(m->magic));
	if (len < required_len) {
		/* can't even look inside the message */
		CERROR("msg length %d too small for magic/version check\n",
			len);
		return (-EINVAL);
	}

	flipped = pscrpc_msg_swabbed(m);
	if (flipped)
		__swab32s(&m->version);
	else if (m->magic != PSCRPC_MSG_MAGIC) {
		CERROR("wrong pscrpc_msg magic %#08x\n", m->magic);
		return (-EINVAL);
	}

#if 0
	if ((m->version & ~LUSTRE_VERSION_MASK) != PSCRPC_MSG_VERSION) {
		CERROR("wrong pscrpc_msg version %#08x\n", m->version);
		return (-EINVAL);
	}
#endif

	/* Now we know the sender speaks my language (but possibly flipped)..*/
	required_len = HDR_SIZE(0);
	if (len < required_len) {
		/* can't even look inside the message */
		CERROR("message length %d too small for pscrpc_msg\n", len);
		return (-EINVAL);
	}

	if (flipped) {
		__swab32s(&m->type);
		__swab32s(&m->opc);
		__swab64s(&m->last_xid);
		__swab64s(&m->last_committed);
		__swab64s(&m->transno);
		__swab32s(&m->status);
		__swab32s(&m->flags);
		__swab32s(&m->conn_cnt);
		__swab32s(&m->bufcount);
	}

	required_len = HDR_SIZE(m->bufcount);

	if (len < required_len) {
		/* didn't receive all the buffer lengths */
		CERROR("message length %d too small for %d buflens\n",
			len, m->bufcount);
		return (-EINVAL);
	}

	for (i = 0; i < (int)m->bufcount; i++) {
		if (flipped)
			__swab32s(&m->buflens[i]);
		required_len += size_round(m->buflens[i]);
	}

	if (len < required_len) {
		CERROR("len: %d, required_len %d\n", len, required_len);
		CERROR("bufcount: %d\n", m->bufcount);
		for (i = 0; i < (int)m->bufcount; i++)
			CERROR("buffer %d length %d\n", i, m->buflens[i]);
		return (-EINVAL);
	}

	return (0);
}

/**
 * pscrpc_msg_buflen - return the length of buffer @n in message @m
 * @m - pscrpc_msg (request or reply) to look at
 * @n - message index (base 0)
 *
 * returns zero for non-existent message indices
 */
int
pscrpc_msg_buflen(struct pscrpc_msg *m, int n)
{
	if (n >= (int)m->bufcount)
		return 0;

	return m->buflens[n];
}
//EXPORT_SYMBOL(pscrpc_msg_buflen);

/**
 * pscrpc_msg_buf -
 * @m:
 * @n:
 * @min_size:
 */
void *
pscrpc_msg_buf(struct pscrpc_msg *m, int n, int min_size)
{
	int i;
	int offset;
	int buflen;
	int bufcount;

	LASSERT(m != NULL);
	LASSERT(n >= 0);

	bufcount = m->bufcount;
	if (n >= bufcount) {
		CDEBUG(D_INFO, "msg %p op %d buffer[%d] not present (count %d)\n",
		       m, m->opc, n, bufcount);
		return NULL;
	}

	buflen = m->buflens[n];
	if (buflen < min_size) {
		CERROR("msg %p op %d buffer[%d] size %d too small (required %d)\n",
		       m, m->opc, n, buflen, min_size);
		return NULL;
	}

	offset = HDR_SIZE(bufcount);
	for (i = 0; i < n; i++)
		offset += size_round(m->buflens[i]);

	return (char *)m + offset;
}

char *
pscrpc_msg_string(struct pscrpc_msg *m, int idx, int max_len)
{
	/* max_len == 0 means the string should fill the buffer */
	char *str = pscrpc_msg_buf(m, idx, 0);
	int   slen;
	int   blen;

	if (str == NULL) {
		CERROR("can't unpack string in msg %p buffer[%d]\n",
			m, idx);
		return (NULL);
	}

	blen = m->buflens[idx];
	slen = strnlen(str, blen);

	if (slen == blen) {                     /* not NULL terminated */
		CERROR("can't unpack non-NULL terminated string in "
			"msg %p buffer[%d] len %d\n", m, idx, blen);
		return (NULL);
	}

	if (max_len == 0) {
		if (slen != blen - 1) {
			CERROR("can't unpack short string in msg %p "
				"buffer[%d] len %d: strlen %d\n",
				m, idx, blen, slen);
			return (NULL);
		}
	} else if (slen > max_len) {
		CERROR("can't unpack oversized string in msg %p "
			"buffer[%d] len %d strlen %d: max %d expected\n",
			m, idx, blen, slen, max_len);
		return (NULL);
	}

	return (str);
}

#if 0  //Ptl only sm
/* Wrap up the normal fixed length cases */
void *
pscrpc_swab_buf(struct pscrpc_msg *msg, int index, int min_size,
		    void *swabber)
{
	void *ptr;

	ptr = pscrpc_msg_buf(msg, index, min_size);
	if (ptr == NULL)
		return NULL;

	if (swabber != NULL && pscrpc_msg_swabbed(msg))
		((void (*)(void *))swabber)(ptr);

	return ptr;
}

void *
pscrpc_swab_reqbuf(struct pscrpc_request *req, int index, int min_size,
			 void *swabber)
{
	LASSERT_REQSWAB(req, index);
	return pscrpc_swab_buf(req->rq_reqmsg, index, min_size, swabber);
}

void *
pscrpc_swab_repbuf(struct pscrpc_request *req, int index, int min_size,
			 void *swabber)
{
	LASSERT_REPSWAB(req, index);
	return pscrpc_swab_buf(req->rq_repmsg, index, min_size, swabber);
}

#endif
