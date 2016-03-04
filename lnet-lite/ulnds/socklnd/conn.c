/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.lustre.org/lustre/docs/GPLv2.pdf
 *
 * Please contact Xyratex Technology, Ltd., Langstone Road, Havant, Hampshire.
 * PO9 1SA, U.K. or visit www.xyratex.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2002, 2013, Xyratex Technology, Ltd . All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Some portions of Lustre® software are subject to copyrights help by Intel Corp.
 * Copyright (c) 2011-2013 Intel Corporation, Inc.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre® and the Lustre logo are registered trademarks of
 * Xyratex Technology, Ltd  in the United States and/or other countries.
 *
 * lnet/ulnds/socklnd/conn.c
 *
 * Author: Maxim Patlasov <maxim@clusterfs.com>
 */

#include <sys/types.h>
#include <netinet/in.h>

#include "usocklnd.h"

#include "pfl/pool.h"

/* Return 1 if the conn is timed out, 0 else */
int
usocklnd_conn_timed_out(usock_conn_t *conn, cfs_time_t current_time)
{
        if (conn->uc_tx_flag && /* sending is in progress */
            cfs_time_aftereq(current_time, conn->uc_tx_deadline))
                return 1;

        if (conn->uc_rx_flag && /* receiving is in progress */
            cfs_time_aftereq(current_time, conn->uc_rx_deadline))
                return 1;
        
        return 0;
}

void
usocklnd_conn_kill(usock_conn_t *conn)
{
        pthread_mutex_lock(&conn->uc_lock);
        if (conn->uc_state != UC_DEAD)
                usocklnd_conn_kill_locked(conn);
        pthread_mutex_unlock(&conn->uc_lock);        
}

/* Mark the conn as DEAD and schedule its deletion */
void
usocklnd_conn_kill_locked(usock_conn_t *conn)
{
        conn->uc_rx_flag = conn->uc_tx_flag = 0;
        conn->uc_state = UC_DEAD;
        usocklnd_add_killrequest(conn);
}

usock_conn_t *
usocklnd_conn_allocate()
{
        usock_conn_t        *conn;
        usock_pollrequest_t *pr;

	pr = psc_pool_get(usk_pollreq_pool);
	memset(pr, 0, sizeof(*pr));
	INIT_PSC_LISTENTRY(&pr->upr_lentry);
        
	conn = psc_pool_get(usk_conn_pool);
        memset(conn, 0, sizeof(*conn));
	INIT_PSC_LISTENTRY(&conn->uc_lentry);
        conn->uc_preq = pr;

        LIBCFS_ALLOC (conn->uc_rx_hello,
                      offsetof(ksock_hello_msg_t,
                               kshm_ips[LNET_MAX_INTERFACES]));
        if (conn->uc_rx_hello == NULL) {
		psc_pool_return(usk_pollreq_pool, pr);
		psc_pool_return(usk_conn_pool, conn);
                return NULL;
        } 

        return conn;
}

void
usocklnd_conn_free(usock_conn_t *conn)
{
        usock_pollrequest_t *pr = conn->uc_preq;

        LASSERT(conn->uc_lx == NULL);

        if (pr != NULL)
		psc_pool_return(usk_pollreq_pool, pr);

        if (conn->uc_rx_hello != NULL)
                LIBCFS_FREE (conn->uc_rx_hello,
                             offsetof(ksock_hello_msg_t,
                                      kshm_ips[LNET_MAX_INTERFACES]));
        
	pthread_mutex_destroy(&conn->uc_lock); 

	psc_pool_return(usk_conn_pool, conn);
}

void
usocklnd_tear_peer_conn(usock_conn_t *conn)
{
        usock_peer_t     *peer = conn->uc_peer;
        int               idx = usocklnd_type2idx(conn->uc_type);
        lnet_ni_t        *ni;
        lnet_process_id_t id;
        int               decref_flag  = 0;
        int               killall_flag = 0;
        void             *rx_lnetmsg   = NULL; 
        CFS_LIST_HEAD    (zombie_txs);
	struct pfl_opstat *opst;
	char pridbuf[LNET_NIDSTR_SIZE];
        
        if (peer == NULL) /* nothing to tear */
                return;
        
        pthread_mutex_lock(&peer->up_lock);
        pthread_mutex_lock(&conn->uc_lock);        

        ni = peer->up_ni;
        id = peer->up_peerid;

        if (peer->up_conns[idx] == conn) {
                if (conn->uc_rx_state == UC_RX_LNET_PAYLOAD) {
                        /* change state not to finalize twice */
                        conn->uc_rx_state = UC_RX_KSM_HEADER;
                        /* stash lnetmsg while holding locks */
                        rx_lnetmsg = conn->uc_rx_lnetmsg;
                }
                
                /* we cannot finalize txs right now (bug #18844) */
                list_splice_init(&conn->uc_tx_list, &zombie_txs);

                peer->up_conns[idx] = NULL;
                conn->uc_peer = NULL;
                decref_flag = 1;

                if(conn->uc_errored && !peer->up_errored)
                        peer->up_errored = killall_flag = 1;

                /* prevent queueing new txs to this conn */
                conn->uc_errored = 1;
        }
        
        pthread_mutex_unlock(&conn->uc_lock);

        if (killall_flag)
                usocklnd_del_conns_locked(peer);

        pthread_mutex_unlock(&peer->up_lock);
        
	opst = pfl_opstat_initf(OPSTF_BASE10, "peer-%s-disconnects-%s",
	    libcfs_expid2str(id, pridbuf),
	    decref_flag ? "full" : "part");
	pfl_opstat_incr(opst);

        if (!decref_flag)
                return;

        if (rx_lnetmsg != NULL)
                lnet_finalize(ni, rx_lnetmsg, -EIO);

        usocklnd_destroy_txlist(ni, &zombie_txs);

        usocklnd_conn_decref(conn);
        usocklnd_peer_decref(peer);

        usocklnd_check_peer_stale(ni, id);
}

/* Remove peer from hash list if all up_conns[i] is NULL &&
 * hash table is the only consumer of the peer */
void
usocklnd_check_peer_stale(lnet_ni_t *ni, lnet_process_id_t id)
{
        usock_peer_t *peer;
        
        pthread_rwlock_wrlock(&usock_data.ud_peers_lock);
        peer = usocklnd_find_peer_locked(ni, id);

        if (peer == NULL) {
                pthread_rwlock_unlock(&usock_data.ud_peers_lock);
                return;
        }

        if (cfs_atomic_read(&peer->up_refcount) == 2) {
                int i;

                for (i = 0; i < N_CONN_TYPES; i++)
                        LASSERT (peer->up_conns[i] == NULL);

		pfl_opstat_destroy(peer->up_iostats.rd);
		peer->up_iostats.rd = NULL;
		pfl_opstat_destroy(peer->up_iostats.wr); 
		peer->up_iostats.wr = NULL;

                list_del(&peer->up_list);                        

                if (peer->up_errored &&
                    (peer->up_peerid.pid & LNET_PID_USERFLAG) == 0)
                        lnet_notify (peer->up_ni, peer->up_peerid.nid, 0,
                                     cfs_time_seconds(peer->up_last_alive));
                
                usocklnd_peer_decref(peer);
        }

        usocklnd_peer_decref(peer);
        pthread_rwlock_unlock(&usock_data.ud_peers_lock);
}

/* Returns 0 on success, <0 else */
int
usocklnd_create_passive_conn(lnet_ni_t *ni, struct lnet_xport *lx,
    usock_conn_t **connp)
{
        int           rc;
        __u32         peer_ip;
        __u16         peer_port;
        usock_conn_t *conn;

        rc = libcfs_getpeername(lx->lx_fd, &peer_ip, &peer_port);
        if (rc)
                return rc;

        rc = usocklnd_set_sock_options(lx->lx_fd, INADDR_ANY);
        if (rc)
                return rc;

        conn = usocklnd_conn_allocate();
        if (conn == NULL)
                return -ENOMEM;

        usocklnd_rx_hellomagic_state_transition(conn);

        conn->uc_lx = lx;
        conn->uc_peer_ip = peer_ip;
        conn->uc_peer_port = peer_port;
        conn->uc_state = UC_RECEIVING_HELLO;
        conn->uc_pt_idx = usocklnd_ip2pt_idx(peer_ip);
        conn->uc_ni = ni;
        CFS_INIT_LIST_HEAD (&conn->uc_tx_list);
        CFS_INIT_LIST_HEAD (&conn->uc_zcack_list);
        pthread_mutex_init(&conn->uc_lock, NULL);
        cfs_atomic_set(&conn->uc_refcount, 1); /* 1 ref for me */

        *connp = conn;
        return 0;
}

/* Returns 0 on success, <0 else */
int
usocklnd_create_active_conn(usock_peer_t *peer, int type,
                            usock_conn_t **connp)
{
        int           rc;
        int           fd;
        usock_conn_t *conn;
        __u32         dst_ip   = LNET_NIDADDR(peer->up_peerid.nid);
        __u16         dst_port = usocklnd_get_cport();
        
        conn = usocklnd_conn_allocate();
        if (conn == NULL)
                return -ENOMEM;

	conn->uc_tx_hello = usocklnd_create_cr_hello_tx(peer->up_ni, type,
							peer->up_peerid.nid);
        if (conn->uc_tx_hello == NULL) {
                usocklnd_conn_free(conn);
                return -ENOMEM;
        }                
        
        if (the_lnet.ln_pid & LNET_PID_USERFLAG)
                rc = usocklnd_connect_cli_mode(&fd,
		    peer->up_peerid.nid, dst_ip, dst_port,
		    LNET_NIDADDR(peer->up_ni->ni_nid));
        else
		rc = usocklnd_connect_srv_mode(&fd,
		    peer->up_peerid.nid, dst_ip, dst_port);
        
        if (rc) {
                usocklnd_destroy_tx(NULL, conn->uc_tx_hello);
                usocklnd_conn_free(conn);
                return rc;
        }
        
        conn->uc_tx_deadline = cfs_time_shift(usock_tuns.ut_timeout);
        conn->uc_tx_flag = 1;
        
        conn->uc_lx = lx_new(LNET_NETTYP(peer->up_peerid.nid) == SSLLND ?
	    &libcfs_ssl_lxi : &libcfs_sock_lxi);
	lx_connect(conn->uc_lx, fd);
        conn->uc_peer_ip = dst_ip;
        conn->uc_peer_port = dst_port;
        conn->uc_type = type;
        conn->uc_activeflag = 1;
        conn->uc_state = UC_CONNECTING;
        conn->uc_pt_idx = usocklnd_ip2pt_idx(dst_ip);
        conn->uc_ni = NULL;
        conn->uc_peerid = peer->up_peerid;
        conn->uc_peer = peer;
        usocklnd_peer_addref(peer);
        CFS_INIT_LIST_HEAD (&conn->uc_tx_list);
        CFS_INIT_LIST_HEAD (&conn->uc_zcack_list);
        pthread_mutex_init(&conn->uc_lock, NULL);
        cfs_atomic_set(&conn->uc_refcount, 1); /* 1 ref for me */

        *connp = conn;
        return 0;
}

/* Returns 0 on success, <0 else */
int
usocklnd_connect_srv_mode(int *fdp, lnet_nid_t nid, __u32 dst_ip,
    __u16 dst_port)
{
        __u16 port;
        int   fd;
        int   rc;

        for (port = LNET_ACCEPTOR_MAX_RESERVED_PORT; 
             port >= LNET_ACCEPTOR_MIN_RESERVED_PORT; 
             port--) {
                /* Iterate through reserved ports. */

                rc = libcfs_sock_create(&fd, nid);
                if (rc)
                        return rc;                        
                                
                rc = libcfs_sock_bind_to_port(fd, nid, INADDR_ANY, port);
                if (rc) {
                        close(fd);
                        continue;
                }

                rc = usocklnd_set_sock_options(fd, dst_ip);
                if (rc) {
                        close(fd);
                        return rc;
                }

                rc = libcfs_sock_connect(fd, dst_ip, dst_port);
                if (rc == 0) {
                        *fdp = fd;
                        return 0;
                }
                
                if (rc != -EADDRINUSE && rc != -EADDRNOTAVAIL) {
                        close(fd);
                        return rc;
                }

                close(fd);
        }

        CERROR("Can't bind to any reserved port\n");
        return rc;
}

/* Returns 0 on success, <0 else */
int
usocklnd_connect_cli_mode(int *fdp, lnet_nid_t nid, __u32 dst_ip,
    __u16 dst_port, __u32 src_ip)
{
        int fd;
        int rc;

        rc = libcfs_sock_create(&fd, nid);
        if (rc)
                return rc;

	if (src_ip != INADDR_ANY)
		libcfs_sock_bind_to_port(fd, nid, src_ip, 0);
        
        rc = usocklnd_set_sock_options(fd, dst_ip);
        if (rc) {
                close(fd);
                return rc;
        }

        rc = libcfs_sock_connect(fd, dst_ip, dst_port);
        if (rc) {
                close(fd);
                return rc;
        }

        *fdp = fd;
        return 0;
}

int
usocklnd_set_sock_options(int fd, __u32 dst_ip)
{
	struct maxseg_range *msr;
        int rc;

        if (usock_tuns.ut_keepalive) {
		rc = libcfs_sock_set_keepalive(fd,
		    usock_tuns.ut_keepalive,
		    usock_tuns.ut_keepalive_cnt,
		    usock_tuns.ut_keepalive_idle,
		    usock_tuns.ut_keepalive_intv);
		if (rc)
			return rc;
	}

        rc = libcfs_sock_set_nagle(fd, usock_tuns.ut_socknagle);
        if (rc)
                return rc;

	psclist_for_each_entry(msr, &usock_tuns.ut_maxsegs, msr_lentry) {
		if ((dst_ip & msr->msr_mask) == msr->msr_net) {
			rc = libcfs_sock_set_maxseg(fd,
			    msr->msr_maxseg);
			if (rc)
				return rc;
		}
	}

        if (usock_tuns.ut_sockbufsiz) {
                rc = libcfs_sock_set_bufsiz(fd, usock_tuns.ut_sockbufsiz);
                if (rc)
                        return rc;        
        }
        
        return libcfs_fcntl_nonblock(fd);
}

usock_tx_t *
usocklnd_create_noop_tx(__u64 cookie)
{
        usock_tx_t *tx;
        
        LIBCFS_ALLOC (tx, sizeof(usock_tx_t));
        if (tx == NULL)
                return NULL;

        tx->tx_size = sizeof(usock_tx_t);
        tx->tx_lnetmsg = NULL;

        socklnd_init_msg(&tx->tx_msg, KSOCK_MSG_NOOP);
        tx->tx_msg.ksm_zc_cookies[1] = cookie;
        
        tx->tx_iova[0].iov_base = (void *)&tx->tx_msg;
        tx->tx_iova[0].iov_len = tx->tx_resid = tx->tx_nob =
                offsetof(ksock_msg_t, ksm_u.lnetmsg.ksnm_hdr);
        tx->tx_iov = tx->tx_iova;
        tx->tx_niov = 1;
        
        return tx;
}
        
usock_tx_t *
usocklnd_create_tx(lnet_msg_t *lntmsg)
{
        usock_tx_t   *tx;
        unsigned int  payload_niov = lntmsg->msg_niov; 
        struct iovec *payload_iov = lntmsg->msg_iov; 
        unsigned int  payload_offset = lntmsg->msg_offset;
        unsigned int  payload_nob = lntmsg->msg_len;
        int           size = offsetof(usock_tx_t,
                                      tx_iova[1 + payload_niov]);

        LIBCFS_ALLOC (tx, size);
        if (tx == NULL)
                return NULL;

        tx->tx_size = size;
        tx->tx_lnetmsg = lntmsg;

        tx->tx_resid = tx->tx_nob =
                offsetof(ksock_msg_t,  ksm_u.lnetmsg.ksnm_payload) +
                payload_nob;
        
        socklnd_init_msg(&tx->tx_msg, KSOCK_MSG_LNET);
        tx->tx_msg.ksm_u.lnetmsg.ksnm_hdr = lntmsg->msg_hdr;
        tx->tx_iova[0].iov_base = (void *)&tx->tx_msg;
        tx->tx_iova[0].iov_len = offsetof(ksock_msg_t,
                                          ksm_u.lnetmsg.ksnm_payload);
        tx->tx_iov = tx->tx_iova;

        tx->tx_niov = 1 + 
                lnet_extract_iov(payload_niov, &tx->tx_iov[1],
                                 payload_niov, payload_iov,
                                 payload_offset, payload_nob);

        return tx;
}

void
usocklnd_init_hello_msg(ksock_hello_msg_t *hello,
                        lnet_ni_t *ni, int type, lnet_nid_t peer_nid)
{
        usock_net_t *net = (usock_net_t *)ni->ni_data;

        hello->kshm_magic       = LNET_PROTO_MAGIC;
        hello->kshm_version     = KSOCK_PROTO_V2;
        hello->kshm_nips        = 0;
        hello->kshm_ctype       = type;
        
        hello->kshm_dst_incarnation = 0; /* not used */
        hello->kshm_src_incarnation = net->un_incarnation;

        hello->kshm_src_pid = the_lnet.ln_pid;
        hello->kshm_src_nid = ni->ni_nid;
        hello->kshm_dst_nid = peer_nid;
        hello->kshm_dst_pid = 0; /* not used */
}

usock_tx_t *
usocklnd_create_hello_tx(lnet_ni_t *ni,
                         int type, lnet_nid_t peer_nid)
{
        usock_tx_t        *tx;
        int                size;
        ksock_hello_msg_t *hello;

        size = sizeof(usock_tx_t) + offsetof(ksock_hello_msg_t, kshm_ips);
        LIBCFS_ALLOC (tx, size);
        if (tx == NULL)
                return NULL;

        tx->tx_size = size;
        tx->tx_lnetmsg = NULL;

        hello = (ksock_hello_msg_t *)&tx->tx_iova[1];
        usocklnd_init_hello_msg(hello, ni, type, peer_nid);
        
        tx->tx_iova[0].iov_base = (void *)hello;
        tx->tx_iova[0].iov_len = tx->tx_resid = tx->tx_nob =
                offsetof(ksock_hello_msg_t, kshm_ips);
        tx->tx_iov = tx->tx_iova;
        tx->tx_niov = 1;

        return tx;
}

usock_tx_t *
usocklnd_create_cr_hello_tx(lnet_ni_t *ni,
                            int type, lnet_nid_t peer_nid)
{
        usock_tx_t              *tx;
        int                      size;
        lnet_acceptor_connreq_t *cr;
        ksock_hello_msg_t       *hello;

        size = sizeof(usock_tx_t) +
                sizeof(lnet_acceptor_connreq_t) +
                offsetof(ksock_hello_msg_t, kshm_ips);
        LIBCFS_ALLOC (tx, size);
        if (tx == NULL)
                return NULL;

        tx->tx_size = size;
        tx->tx_lnetmsg = NULL;

        cr = (lnet_acceptor_connreq_t *)&tx->tx_iova[1];
        memset(cr, 0, sizeof(*cr));
        cr->acr_magic   = LNET_PROTO_ACCEPTOR_MAGIC;
        cr->acr_version = LNET_PROTO_ACCEPTOR_VERSION;
        cr->acr_nid     = peer_nid;
        
        hello = (ksock_hello_msg_t *)((char *)cr + sizeof(*cr));
        usocklnd_init_hello_msg(hello, ni, type, peer_nid);
        
        tx->tx_iova[0].iov_base = (void *)cr;
        tx->tx_iova[0].iov_len = tx->tx_resid = tx->tx_nob =
                sizeof(lnet_acceptor_connreq_t) +
                offsetof(ksock_hello_msg_t, kshm_ips);
        tx->tx_iov = tx->tx_iova;
        tx->tx_niov = 1;

        return tx;
}

void
usocklnd_destroy_tx(lnet_ni_t *ni, usock_tx_t *tx)
{
        lnet_msg_t  *lnetmsg = tx->tx_lnetmsg;
        int          rc = (tx->tx_resid == 0) ? 0 : -EIO;

        LASSERT (ni != NULL || lnetmsg == NULL);

        LIBCFS_FREE (tx, tx->tx_size);
        
        if (lnetmsg != NULL) /* NOOP and hello go without lnetmsg */
                lnet_finalize(ni, lnetmsg, rc);
}

void
usocklnd_destroy_txlist(lnet_ni_t *ni, struct list_head *txlist)
{
        usock_tx_t *tx;

        while (!list_empty(txlist)) {
                tx = list_entry(txlist->next, usock_tx_t, tx_list);
                list_del(&tx->tx_list);
                
                usocklnd_destroy_tx(ni, tx);
        }
}

void
usocklnd_destroy_zcack_list(struct list_head *zcack_list)
{
        usock_zc_ack_t *zcack;

        while (!list_empty(zcack_list)) {
                zcack = list_entry(zcack_list->next, usock_zc_ack_t, zc_list);
                list_del(&zcack->zc_list);
                
                LIBCFS_FREE (zcack, sizeof(*zcack));
        }
}

void
usocklnd_destroy_peer(usock_peer_t *peer)
{
        usock_net_t *net = peer->up_ni->ni_data;
        int          i;
	lnet_event_t ev;
	lnet_eq_t *eq;

        for (i = 0; i < N_CONN_TYPES; i++)
                LASSERT (peer->up_conns[i] == NULL);

	psc_assert(peer->up_iostats.rd == NULL);
	psc_assert(peer->up_iostats.wr == NULL); 

	/* Inform all eq's to drop associations to this peer. */
	LNET_LOCK();
	list_for_each_entry(eq, &the_lnet.ln_active_eqs, eq_list) {
		memset(&ev, 0, sizeof(ev));
		ev.type = LNET_EVENT_DROP;
		ev.initiator.nid = peer->up_peerid.nid;
		ev.initiator.pid = peer->up_peerid.pid;
		lnet_enq_event_locked(eq, &ev);
	}
	LNET_UNLOCK(); 

	pthread_mutex_destroy(&peer->up_lock); 

	psc_pool_return(usk_peer_pool, peer);

        pthread_mutex_lock(&net->un_lock);
        if(--net->un_peercount == 0)                
                pthread_cond_signal(&net->un_cond);
        pthread_mutex_unlock(&net->un_lock);
}

void
usocklnd_destroy_conn(usock_conn_t *conn)
{
        LASSERT (conn->uc_peer == NULL || conn->uc_ni == NULL);

        if (conn->uc_rx_state == UC_RX_LNET_PAYLOAD) {
                LASSERT (conn->uc_peer != NULL);
                lnet_finalize(conn->uc_peer->up_ni, conn->uc_rx_lnetmsg, -EIO);
        }

        if (!list_empty(&conn->uc_tx_list)) {
                LASSERT (conn->uc_peer != NULL);                
                usocklnd_destroy_txlist(conn->uc_peer->up_ni, &conn->uc_tx_list);
        }

        usocklnd_destroy_zcack_list(&conn->uc_zcack_list);
        
        if (conn->uc_peer != NULL)
                usocklnd_peer_decref(conn->uc_peer);

        if (conn->uc_ni != NULL)
                lnet_ni_decref(conn->uc_ni);

        if (conn->uc_tx_hello)
                usocklnd_destroy_tx(NULL, conn->uc_tx_hello);

        usocklnd_conn_free(conn);
}

int
usocklnd_get_conn_type(lnet_msg_t *lntmsg)
{
        int nob;

        if (the_lnet.ln_pid & LNET_PID_USERFLAG)
                return SOCKLND_CONN_ANY;

        nob = offsetof(ksock_msg_t, ksm_u.lnetmsg.ksnm_payload) +
                lntmsg->msg_len;
        
        if (nob >= usock_tuns.ut_min_bulk)
                return SOCKLND_CONN_BULK_OUT;
        else
                return SOCKLND_CONN_CONTROL;
}

int usocklnd_type2idx(int type)
{
        switch (type) {
        case SOCKLND_CONN_ANY:
        case SOCKLND_CONN_CONTROL:
                return 0;
        case SOCKLND_CONN_BULK_IN:
                return 1;
        case SOCKLND_CONN_BULK_OUT:
                return 2;
        default:
                LBUG();
                return 0; /* make compiler happy */
        }
}

usock_peer_t *
usocklnd_find_peer_locked(lnet_ni_t *ni, lnet_process_id_t id)
{
        struct list_head *peer_list = usocklnd_nid2peerlist(id.nid);
        struct list_head *tmp;
        usock_peer_t     *peer;

        list_for_each (tmp, peer_list) {

                peer = list_entry (tmp, usock_peer_t, up_list);

                if (peer->up_ni != ni)
                        continue;

                if (peer->up_peerid.nid != id.nid ||
                    peer->up_peerid.pid != id.pid)
                        continue;

                usocklnd_peer_addref(peer);
                return peer;
        }
        return (NULL);
}

int
usocklnd_create_peer(lnet_ni_t *ni, lnet_process_id_t id,
                     usock_peer_t **peerp)
{
        usock_net_t  *net = ni->ni_data;
        usock_peer_t *peer;
        int           i;

	peer = psc_pool_get(usk_peer_pool);
	memset(peer, 0, sizeof(*peer));
	INIT_PSC_LISTENTRY(&peer->up_lentry);

        for (i = 0; i < N_CONN_TYPES; i++)
                peer->up_conns[i] = NULL;

        peer->up_peerid       = id;
        peer->up_ni           = ni;
        peer->up_incrn_is_set = 0;
        peer->up_errored      = 0;
        peer->up_last_alive   = 0;
        cfs_atomic_set (&peer->up_refcount, 1); /* 1 ref for caller */
        pthread_mutex_init(&peer->up_lock, NULL);        

        pthread_mutex_lock(&net->un_lock);
        net->un_peercount++;        
        pthread_mutex_unlock(&net->un_lock);

        *peerp = peer;
        return 0;
}

/* Safely create new peer if needed. Save result in *peerp.
 * Returns 0 on success, <0 else */
int
usocklnd_find_or_create_peer(lnet_ni_t *ni, lnet_process_id_t id,
                             usock_peer_t **peerp)
{
        int           rc;
        usock_peer_t *peer;
        usock_peer_t *peer2;
        usock_net_t  *net = ni->ni_data;

        pthread_rwlock_rdlock(&usock_data.ud_peers_lock);
        peer = usocklnd_find_peer_locked(ni, id);
        pthread_rwlock_unlock(&usock_data.ud_peers_lock);

        if (peer != NULL)
                goto find_or_create_peer_done;

        rc = usocklnd_create_peer(ni, id, &peer);
        if (rc)
                return rc;
        
        pthread_rwlock_wrlock(&usock_data.ud_peers_lock);
        peer2 = usocklnd_find_peer_locked(ni, id);
        if (peer2 == NULL) {
		char pridbuf[LNET_NIDSTR_SIZE];

                if (net->un_shutdown) {
                        pthread_rwlock_unlock(&usock_data.ud_peers_lock);
                        usocklnd_peer_decref(peer); /* should destroy peer */
                        CERROR("Can't create peer: network shutdown\n");
                        return -ESHUTDOWN;
                }

		peer->up_iostats.rd = pfl_opstat_initf(OPSTF_EXCL,
		    "peer-%s-rcv", libcfs_expid2str(id, pridbuf));
		peer->up_iostats.wr = pfl_opstat_initf(OPSTF_EXCL,
		    "peer-%s-snd", libcfs_expid2str(id, pridbuf));
                
                /* peer table will take 1 of my refs on peer */
                usocklnd_peer_addref(peer);
                list_add_tail (&peer->up_list,
                               usocklnd_nid2peerlist(id.nid));
        } else {
                usocklnd_peer_decref(peer); /* should destroy peer */
                peer = peer2;
        }
        pthread_rwlock_unlock(&usock_data.ud_peers_lock);
        
  find_or_create_peer_done:        
        *peerp = peer;
        return 0;
}

/* NB: both peer and conn locks are held */
static int
usocklnd_enqueue_zcack(usock_conn_t *conn, usock_zc_ack_t *zc_ack)
{        
        if (conn->uc_state == UC_READY &&
            list_empty(&conn->uc_tx_list) &&
            list_empty(&conn->uc_zcack_list) &&
            !conn->uc_sending) {
                int rc = usocklnd_add_pollrequest(conn, POLL_TX_SET_REQUEST,
                                                  POLLOUT);
                if (rc != 0)
                        return rc;
        }                

        list_add_tail(&zc_ack->zc_list, &conn->uc_zcack_list);
        return 0;
}

/* NB: both peer and conn locks are held
 * NB: if sending isn't in progress.  the caller *MUST* send tx
 * immediately after we'll return */
static void
usocklnd_enqueue_tx(usock_conn_t *conn, usock_tx_t *tx,
                    int *send_immediately)
{        
        if (conn->uc_state == UC_READY &&
            list_empty(&conn->uc_tx_list) &&
            list_empty(&conn->uc_zcack_list) &&
            !conn->uc_sending) {
                conn->uc_sending = 1;
                *send_immediately = 1;
                return;
        }                

        *send_immediately = 0;
        list_add_tail(&tx->tx_list, &conn->uc_tx_list);
}

/* Safely create new conn if needed. Save result in *connp.
 * Returns 0 on success, <0 else */
int
usocklnd_find_or_create_conn(usock_peer_t *peer, int type,
                             usock_conn_t **connp,
                             usock_tx_t *tx, usock_zc_ack_t *zc_ack,
                             int *send_immediately)
{
        usock_conn_t *conn;
        int           idx;
        int           rc;
        lnet_pid_t    userflag = peer->up_peerid.pid & LNET_PID_USERFLAG;
        
        if (userflag)
                type = SOCKLND_CONN_ANY;

        idx = usocklnd_type2idx(type);
        
        pthread_mutex_lock(&peer->up_lock);
        if (peer->up_conns[idx] != NULL) {
                conn = peer->up_conns[idx];
                LASSERT(conn->uc_type == type);
        } else {
#if 0
                if (userflag) {
                        CERROR("Refusing to create a connection to "
                               "userspace process %s\n",
                               libcfs_id2str(peer->up_peerid));
                        rc = -EHOSTUNREACH;
                        goto find_or_create_conn_failed;
                }
#endif
                
                rc = usocklnd_create_active_conn(peer, type, &conn);
                if (rc) {
                        peer->up_errored = 1;
                        usocklnd_del_conns_locked(peer);
                        goto find_or_create_conn_failed;
                }

                /* peer takes 1 of conn refcount */
                usocklnd_link_conn_to_peer(conn, peer, idx);
                
                rc = usocklnd_add_pollrequest(conn, POLL_ADD_REQUEST, POLLOUT);
                if (rc) {
                        peer->up_conns[idx] = NULL;
                        usocklnd_conn_decref(conn); /* should destroy conn */
                        goto find_or_create_conn_failed;
                }
                usocklnd_wakeup_pollthread(conn->uc_pt_idx);
        }
        
        pthread_mutex_lock(&conn->uc_lock);
        LASSERT(conn->uc_peer == peer);

        LASSERT(tx == NULL || zc_ack == NULL);
        if (tx != NULL) {
                /* usocklnd_tear_peer_conn() could signal us stop queueing */
                if (conn->uc_errored) {
                        rc = -EIO;
                        pthread_mutex_unlock(&conn->uc_lock);
                        goto find_or_create_conn_failed;
                }

                usocklnd_enqueue_tx(conn, tx, send_immediately);
        } else {
                rc = usocklnd_enqueue_zcack(conn, zc_ack);        
                if (rc != 0) {
                        usocklnd_conn_kill_locked(conn);
                        pthread_mutex_unlock(&conn->uc_lock);
                        goto find_or_create_conn_failed;
                }
        }
        pthread_mutex_unlock(&conn->uc_lock);         

        usocklnd_conn_addref(conn);
        pthread_mutex_unlock(&peer->up_lock);

        *connp = conn;
        return 0;

  find_or_create_conn_failed:
        pthread_mutex_unlock(&peer->up_lock);
        return rc;
}

void
usocklnd_link_conn_to_peer(usock_conn_t *conn, usock_peer_t *peer, int idx)
{
        peer->up_conns[idx] = conn;        
        peer->up_errored    = 0; /* this new fresh conn will try
                                  * revitalize even stale errored peer */
}

int
usocklnd_invert_type(int type)
{
        switch (type)
        {
        case SOCKLND_CONN_ANY:
        case SOCKLND_CONN_CONTROL:
                return (type);
        case SOCKLND_CONN_BULK_IN:
                return SOCKLND_CONN_BULK_OUT;
        case SOCKLND_CONN_BULK_OUT:
                return SOCKLND_CONN_BULK_IN;
        default:
                return SOCKLND_CONN_NONE;
        }
}

void
usocklnd_conn_new_state(usock_conn_t *conn, int new_state)
{
        pthread_mutex_lock(&conn->uc_lock);
        if (conn->uc_state != UC_DEAD)
                conn->uc_state = new_state;
        pthread_mutex_unlock(&conn->uc_lock);
}

/* NB: peer is locked by caller */
void
usocklnd_cleanup_stale_conns(usock_peer_t *peer, __u64 incrn,
                             usock_conn_t *skip_conn)
{
        int i;
        
        if (!peer->up_incrn_is_set) {
                peer->up_incarnation = incrn;
                peer->up_incrn_is_set = 1;
                return;
        }

        if (peer->up_incarnation == incrn)
                return;

        peer->up_incarnation = incrn;
        
        for (i = 0; i < N_CONN_TYPES; i++) {
                usock_conn_t *conn = peer->up_conns[i];
                
                if (conn == NULL || conn == skip_conn)
                        continue;

                pthread_mutex_lock(&conn->uc_lock);        
                LASSERT (conn->uc_peer == peer);
                conn->uc_peer = NULL;
                peer->up_conns[i] = NULL;
                if (conn->uc_state != UC_DEAD)
                        usocklnd_conn_kill_locked(conn);                
                pthread_mutex_unlock(&conn->uc_lock);

                usocklnd_conn_decref(conn);
                usocklnd_peer_decref(peer);
        }
}

/* RX state transition to UC_RX_HELLO_MAGIC: update RX part to receive
 * MAGIC part of hello and set uc_rx_state
 */
void
usocklnd_rx_hellomagic_state_transition(usock_conn_t *conn)
{
        LASSERT(conn->uc_rx_hello != NULL);

        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_hello->kshm_magic;
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =
                sizeof(conn->uc_rx_hello->kshm_magic);

        conn->uc_rx_state = UC_RX_HELLO_MAGIC;

        conn->uc_rx_flag = 1; /* waiting for incoming hello */
        conn->uc_rx_deadline = cfs_time_shift(usock_tuns.ut_timeout);
}

/* RX state transition to UC_RX_HELLO_VERSION: update RX part to receive
 * VERSION part of hello and set uc_rx_state
 */
void
usocklnd_rx_helloversion_state_transition(usock_conn_t *conn)
{
        LASSERT(conn->uc_rx_hello != NULL);

        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_hello->kshm_version;
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =
                sizeof(conn->uc_rx_hello->kshm_version);
        
        conn->uc_rx_state = UC_RX_HELLO_VERSION;
}

/* RX state transition to UC_RX_HELLO_BODY: update RX part to receive
 * the rest  of hello and set uc_rx_state
 */
void
usocklnd_rx_hellobody_state_transition(usock_conn_t *conn)
{
        LASSERT(conn->uc_rx_hello != NULL);

        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_hello->kshm_src_nid;
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =
                offsetof(ksock_hello_msg_t, kshm_ips) -
                offsetof(ksock_hello_msg_t, kshm_src_nid);
        
        conn->uc_rx_state = UC_RX_HELLO_BODY;
}

/* RX state transition to UC_RX_HELLO_IPS: update RX part to receive
 * array of IPs and set uc_rx_state
 */
void
usocklnd_rx_helloIPs_state_transition(usock_conn_t *conn)
{
        LASSERT(conn->uc_rx_hello != NULL);

        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_hello->kshm_ips;
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =
                conn->uc_rx_hello->kshm_nips *
                sizeof(conn->uc_rx_hello->kshm_ips[0]);
        
        conn->uc_rx_state = UC_RX_HELLO_IPS;
}

/* RX state transition to UC_RX_LNET_HEADER: update RX part to receive
 * LNET header and set uc_rx_state
 */
void
usocklnd_rx_lnethdr_state_transition(usock_conn_t *conn)
{
        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_msg.ksm_u.lnetmsg;                
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =
                sizeof(ksock_lnet_msg_t);
        
        conn->uc_rx_state = UC_RX_LNET_HEADER;
        conn->uc_rx_flag = 1;
}

/* RX state transition to UC_RX_KSM_HEADER: update RX part to receive
 * KSM header and set uc_rx_state
 */
void
usocklnd_rx_ksmhdr_state_transition(usock_conn_t *conn)
{
        conn->uc_rx_niov = 1;
        conn->uc_rx_iov = conn->uc_rx_iova;
        conn->uc_rx_iov[0].iov_base = &conn->uc_rx_msg;                
        conn->uc_rx_iov[0].iov_len =
                conn->uc_rx_nob_wanted =
                conn->uc_rx_nob_left =                        
                offsetof(ksock_msg_t, ksm_u);
        
        conn->uc_rx_state = UC_RX_KSM_HEADER;
        conn->uc_rx_flag = 0;
}

/* RX state transition to UC_RX_SKIPPING: update RX part for
 * skipping and set uc_rx_state
 */
void
usocklnd_rx_skipping_state_transition(usock_conn_t *conn)
{
        static char    skip_buffer[4096];

        int            nob;
        unsigned int   niov = 0;
        int            skipped = 0;
        int            nob_to_skip = conn->uc_rx_nob_left;
        
        LASSERT(nob_to_skip != 0);

        conn->uc_rx_iov = conn->uc_rx_iova;

        /* Set up to skip as much as possible now.  If there's more left
         * (ran out of iov entries) we'll get called again */

        do {
                nob = MIN (nob_to_skip, (int)sizeof(skip_buffer));

                conn->uc_rx_iov[niov].iov_base = skip_buffer;
                conn->uc_rx_iov[niov].iov_len  = nob;
                niov++;
                skipped += nob;
                nob_to_skip -=nob;

        } while (nob_to_skip != 0 &&    /* mustn't overflow conn's rx iov */
                 niov < sizeof(conn->uc_rx_iova) / sizeof (struct iovec));

        conn->uc_rx_niov = niov;
        conn->uc_rx_nob_wanted = skipped;

        conn->uc_rx_state = UC_RX_SKIPPING;
}
