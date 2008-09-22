/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2002 Cray Inc.
 *  Copyright (c) 2003 Cluster File Systems, Inc.
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* tcpnal.c:
   This file implements the TCP-based nal by providing glue
   between the connection service and the generic NAL implementation */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pqtimer.h>
#include <dispatch.h>
#include <procbridge.h>
#include <connection.h>
#include <errno.h>
#include <sendrecv.h>

#if defined(__APPLE__)
#include <sys/syscall.h>
#elif !defined(__CYGWIN__)
#include <syscall.h>
#endif

/* for __unusedx */
#include "psc_util/cdefs.h"
#include "psc_util/iostats.h"

void
tcpnal_notify(lnet_ni_t *ni, lnet_nid_t nid, int alive)
{
        bridge     b = (bridge)ni->ni_data;
        connection c;

        if (!alive) {
                LBUG();
        }
        CWARN("forcing connection to pid 12345-%s", 
              libcfs_nid2str(nid));
        c = force_tcp_connection_old((manager)b->lower, nid, b->local);
        if (c == NULL)
                CERROR("Can't create connection to %s\n",
                       libcfs_nid2str(nid));
}

/*
 * sends a packet to the peer, after insuring that a connection exists
 */
int tcpnal_send(lnet_ni_t *ni, __unusedx void *private, lnet_msg_t *lntmsg)
{
        lnet_hdr_t        *hdr = &lntmsg->msg_hdr;
        lnet_process_id_t  target = {lntmsg->msg_target.nid, 
                                     lntmsg->msg_target.pid};
        unsigned int       niov = lntmsg->msg_niov;
        struct iovec      *iov = lntmsg->msg_iov;
        unsigned int       offset = lntmsg->msg_offset;
        unsigned int       len = lntmsg->msg_len;

        connection c;
        bridge b = (bridge)ni->ni_data;
        struct iovec *tiov;
        static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
	struct iostats *ist;
        int rc = 0;
        int   ntiov;

	ist = ni->ni_sendstats;

        if (!(c = force_tcp_connection((manager)b->lower, &target,
                                       b->local)))
                return(-EIO);

        /* TODO: these results should be checked. furthermore, provision
           must be made for the SIGPIPE which is delivered when
           writing on a tcp socket which has closed underneath
           the application. there is a linux flag in the sendmsg
           call which turns off the signally behaviour, but its
           nonstandard */

        LASSERT (niov <= 256);
        LASSERT (len == 0 || iov != NULL);      /* I don't understand kiovs */

        tiov = PSCALLOC(sizeof(*tiov) * (niov+1));

        tiov[0].iov_base = hdr;
        tiov[0].iov_len = sizeof(lnet_hdr_t);
        ntiov = 1 + lnet_extract_iov(niov, &tiov[1], niov, iov, offset, len);

        pthread_mutex_lock(&send_lock);
	rc = psc_sock_writev(c->fd, tiov, ntiov, 0);
        pthread_mutex_unlock(&send_lock);

	for (total = 0, j = 0; j < ntiov; j++)
		total += tiov[i].iov_len;

	atomic_add(total, &ist->ist_bytes_intv);
        PSCFREE(tiov);

        CDEBUG(D_NET, "sent %s total %d in %d frags (rc=%d)\n",
               hdr->type == LNET_MSG_ACK ? "ACK" :
               hdr->type == LNET_MSG_PUT ? "PUT" :
               hdr->type == LNET_MSG_GET ? "GET" :
               hdr->type == LNET_MSG_REPLY ? "REPLY" :
               hdr->type == LNET_MSG_HELLO ? "HELLO" : "UNKNOWN",
               total, niov + 1, rc);

        if (rc == 0) {
                /* NB the NAL only calls lnet_finalize() if it returns 0
                 * from cb_send() */
                lnet_finalize(ni, lntmsg, 0);
        } 
        return(rc);
}


int tcpnal_recv(lnet_ni_t     *ni,
                void         *private,
                lnet_msg_t   *cookie,
                __unusedx int delayed,
                unsigned int  niov,
                struct iovec *iov,
                __unusedx lnet_kiov_t  *kiov,
                unsigned int  offset,
                unsigned int  mlen,
                unsigned int  rlen)
{
        struct iovec *tiov = PSCALLOC(sizeof(*tiov) * (niov));
        static pthread_mutex_t recv_lock = PTHREAD_MUTEX_INITIALIZER;
	struct iostats *ist;
        unsigned char *trash;
        int ntiov, rc;
	connection c;

        LASSERT(rlen >= mlen);

	c = private;
	ist = ni->ni_recvstats;
        if (mlen == 0)
                goto finalize;

        LASSERT(iov != NULL);           /* I don't understand kiovs */

        ntiov = lnet_extract_iov(niov, tiov, niov, iov, offset, mlen);

        trash=malloc(rlen - mlen);
        pthread_mutex_lock(&recv_lock);
	rc = psc_sock_readv(c->fd, tiov, ntiov, 0);

        if (mlen != rlen){ 
		CERROR("trashing %d bytes\n", rlen - mlen);
                /*TODO: check error status*/
	        psc_sock_read(c->fd, trash, rlen - mlen, 0);
        }

        pthread_mutex_unlock(&recv_lock);

        PSCFREE(tiov); 
	free(trash);
	atomic_add(rlen, &ist->ist_bytes_intv);
finalize:

	if (rc == 0)
	        lnet_finalize(ni, cookie, 0); 
        return (rc);
}


/* Function:  from_connection:
 * Arguments: c: the connection to read from
 * Returns: whether or not to continue reading from this connection,
 *          expressed as a 1 to continue, and a 0 to not
 *
 *  from_connection() is called from the select loop when i/o is
 *  available. It attempts to read the portals header and
 *  pass it to the generic library for processing.
 */
static int from_connection(void *a, void *d)
{
        connection c = d;
        bridge     b = a;
        lnet_hdr_t hdr;
        int  rc;

        memset(&hdr, 0, sizeof(lnet_hdr_t));

        if (psc_sock_read(c->fd, &hdr, sizeof(hdr), 0) == 0) {
                CDEBUG(D_NET, "SRC %s:%u DEST %s CONNNID %s %u\n",
                       libcfs_nid2str(hdr.src_nid),
                       hdr.src_pid,
                       libcfs_nid2str(hdr.dest_nid),
                       libcfs_nid2str(c->peer_nid), 
                       hdr.type);
                /* replace dest_nid,pid (socknal sets its own) */
                hdr.dest_nid = cpu_to_le64(b->b_ni->ni_nid);
                hdr.dest_pid = cpu_to_le32(the_lnet.ln_pid);

                rc = lnet_parse(b->b_ni, &hdr, c->peer_nid, c, 0);
                //rc = lnet_parse(b->b_ni, &hdr, hdr.src_nid, c, 0);
                if (rc < 0) {
                        CERROR("Error %d from lnet_parse\n", rc);
                        return 0;
                }

                return(1);
        }
        return(0);
}


void tcpnal_shutdown(bridge b)
{
        shutdown_connections(b->lower);
}

/* Function:  PTL_IFACE_TCP
 * Arguments: pid_request: desired port number to bind to
 *            desired: passed NAL limits structure
 *            actual: returned NAL limits structure
 * Returns: a nal structure on success, or null on failure
 */
int tcpnal_init(bridge b)
{
        manager m;

        tcpnal_set_global_params();

        if (!(m = init_connections(from_connection, b))) {
                /* TODO: this needs to shut down the newly created junk */
                return(-ENXIO);
        }
	iostats_init(b->b_ni->ni_recvstats, "lndrcv-%s",
	    libcfs_nid2str(b->b_ni->ni_nid));
	iostats_init(b->b_ni->ni_sendstats, "lndsnd-%s",
	    libcfs_nid2str(b->b_ni->ni_nid));
        b->lower = m;
        return(0);
}
