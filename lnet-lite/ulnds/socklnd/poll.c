/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lnet/ulnds/socklnd/poll.c
 *
 * Author: Maxim Patlasov <maxim@clusterfs.com>
 */

#include "usocklnd.h"
#include <unistd.h>
#include <sys/syscall.h>
#if defined(__sun__) || defined(__sun)
# include <sys/devpoll.h>
#endif

#if defined(__sun__) || defined(__sun)
int
usocklnd_load_devpollrequests(usock_pollthread_t *pt)
{
        int len, wlen;
        int nreq = pt->upt_nrequests;
        
        if (nreq == 0)
                return 0;

        len = nreq * sizeof(struct pollfd);
        wlen = syscall(SYS_write, pt->upt_dp_fd, pt->upt_requests, len);

        if (wlen != len) {
                int rc;
                
                if ( wlen >= 0 ) {
                        rc = -EIO;
                        CERROR("Very unlikely event happend: partial write "
                               "to /dev/poll fd (wlen=%d)\n", wlen);
                } else {
                        rc = -errno;
                        CERROR("Very unlikely event happend: cannot write "
                               "to /dev/poll fd (wlen=%d; errno=%d)\n",
                               wlen, errno);
                }

                return rc;
        }

        pt->upt_nrequests = 0;
        return 0;
}

int
usocklnd_add_devpollrequest(usock_pollthread_t *pt, int fd, int value)
{
        int            rc;
        struct pollfd *pfd;

        /* commit requests[] if no room for the next */
        if (pt->upt_nrequests >= pt->upt_npollfd &&
            (rc = usocklnd_load_devpollrequests(pt)) != 0)
                return rc;
                     
        pfd = &pt->upt_requests[pt->upt_nrequests++];
        pfd->fd = fd;
        pfd->events = value;
        pfd->revents = 0;

        return 0;
}
#endif

void
usocklnd_process_stale_list(usock_pollthread_t *pt_data)
{
        while (!list_empty(&pt_data->upt_stale_list)) {
                usock_conn_t *conn;                        
                conn = list_entry(pt_data->upt_stale_list.next,
                                  usock_conn_t, uc_stale_list);
                
                list_del(&conn->uc_stale_list);
                
                usocklnd_tear_peer_conn(conn);
                usocklnd_conn_decref(conn); /* -1 for idx2conn[idx] or pr */
        }
}

int
usocklnd_poll_thread(void *arg)
{
        int                 rc = 0;
        usock_pollthread_t *pt_data = (usock_pollthread_t *)arg;
        cfs_time_t          current_time;
        cfs_time_t          planned_time;
        int                 idx;
        int                 idx_start;
        int                 idx_finish;
        int                 chunk;
        int                 saved_nfds;
        int                 extra;
        int                 times;
	struct lnet_xport  *lx;
#if defined(__sun__) || defined(__sun)
        struct dvpoll       dvp;
#endif
        
        /* mask signals to avoid SIGPIPE, etc */
        sigset_t  sigs;
        sigfillset (&sigs);
        pthread_sigmask (SIG_SETMASK, &sigs, 0);
        
        LASSERT(pt_data != NULL);
        
        planned_time = cfs_time_shift(usock_tuns.ut_poll_timeout);
        chunk = usocklnd_calculate_chunk_size(pt_data->upt_nfds);
        saved_nfds = pt_data->upt_nfds;
        idx_start = 1;
        
        /* Main loop */
        while (usock_data.ud_shutdown == 0) {
                rc = 0;

                /* Process all enqueued poll requests */
                pthread_mutex_lock(&pt_data->upt_pollrequests_lock);
                while (!list_empty(&pt_data->upt_pollrequests)) {
                        usock_pollrequest_t *pr;
                        pr = list_entry(pt_data->upt_pollrequests.next,
                                        usock_pollrequest_t, upr_list);
                        
                        list_del(&pr->upr_list);
                        rc = usocklnd_process_pollrequest(pr, pt_data);
                        if (rc)
                                break;                        
                }
                pthread_mutex_unlock(&pt_data->upt_pollrequests_lock);

                if (rc)
                        break;

                /* Delete conns orphaned due to POLL_DEL_REQUESTs */
                usocklnd_process_stale_list(pt_data);

#if defined(__sun__) || defined(__sun)                
                /* Commit outstanding requests to /dev/poll */
                rc = usocklnd_load_devpollrequests(pt_data);
                if (rc < 0)
                        break;

                dvp.dp_fds = pt_data->upt_results;
                dvp.dp_nfds = pt_data->upt_npollfd;
                dvp.dp_timeout = usock_tuns.ut_poll_timeout * 1000;
                
                /* Actual /dev/poll-ing for events */
                rc = ioctl(pt_data->upt_dp_fd, DP_POLL, &dvp);
                pt_data->upt_nresults = rc;
#else
                /* Actual polling for events */
                rc = poll(pt_data->upt_pollfd,
                          pt_data->upt_nfds,
                          usock_tuns.ut_poll_timeout * 1000);

#endif
                if (rc < 0) {
			if (errno == EINTR)
				continue;
                        CERROR("Cannot poll(2): errno=%d\n", errno);
                        break;
                }

                if (rc > 0)
                        usocklnd_execute_handlers(pt_data);

                current_time = cfs_time_current();

                if (pt_data->upt_nfds < 2 ||
                    cfs_time_before(current_time, planned_time))
                        continue;

                /* catch up growing pollfd[] */
                if (pt_data->upt_nfds > saved_nfds) {
                        extra = pt_data->upt_nfds - saved_nfds;
                        saved_nfds = pt_data->upt_nfds;
                } else {
                        extra = 0;
                }

                times = cfs_duration_sec(cfs_time_sub(current_time, planned_time)) + 1;                
                idx_finish = MIN(idx_start + chunk*times + extra, pt_data->upt_nfds);

                for (idx = idx_start; idx < idx_finish; idx++) {
                        usock_conn_t *conn = pt_data->upt_idx2conn[idx];
                        pthread_mutex_lock(&conn->uc_lock);
                        if (usocklnd_conn_timed_out(conn, current_time) &&
                            conn->uc_state != UC_DEAD) {
                                conn->uc_errored = 1;
                                usocklnd_conn_kill_locked(conn);
                        }
                        pthread_mutex_unlock(&conn->uc_lock);
                }

                if (idx_finish == pt_data->upt_nfds) {                        
                        chunk = usocklnd_calculate_chunk_size(pt_data->upt_nfds);
                        saved_nfds = pt_data->upt_nfds;
                        idx_start = 1;
                }
                else {
                        idx_start = idx_finish;
                }
                
                planned_time = cfs_time_add(current_time,
                                            cfs_time_seconds(usock_tuns.ut_poll_timeout));
        }
        
        /* All conns should be deleted by POLL_DEL_REQUESTs while shutdown */
        LASSERT (rc != 0 || pt_data->upt_nfds == 1);

        if (rc) {
                pthread_mutex_lock(&pt_data->upt_pollrequests_lock);

                /* Block new poll requests to be enqueued */
                pt_data->upt_errno = rc;
                
                while (!list_empty(&pt_data->upt_pollrequests)) {
                        usock_pollrequest_t *pr;
                        pr = list_entry(pt_data->upt_pollrequests.next,
                                        usock_pollrequest_t, upr_list);
                        
                        list_del(&pr->upr_list);

                        if (pr->upr_type == POLL_ADD_REQUEST) {
                                lx = pr->upr_conn->uc_lx;
                                pr->upr_conn->uc_lx = NULL;
                                lx_close(lx);
                                close(lx->lx_fd);
                                lx_destroy(lx);

                                list_add_tail(&pr->upr_conn->uc_stale_list,
                                              &pt_data->upt_stale_list);
                        } else {
                                usocklnd_conn_decref(pr->upr_conn);
                        }
                        
                        LIBCFS_FREE (pr, sizeof(*pr));
                }
                pthread_mutex_unlock(&pt_data->upt_pollrequests_lock);

                usocklnd_process_stale_list(pt_data);
                
                for (idx = 1; idx < pt_data->upt_nfds; idx++) {
                        usock_conn_t *conn = pt_data->upt_idx2conn[idx];
                        LASSERT(conn != NULL);

                        lx = conn->uc_lx;
                        conn->uc_lx = NULL;
                        lx_close(lx);
                        close(lx->lx_fd);
                        lx_destroy(lx);

                        usocklnd_tear_peer_conn(conn);
                        usocklnd_conn_decref(conn);
                }
        }
        
        /* unblock usocklnd_shutdown() */
        cfs_complete(&pt_data->upt_completion);

        return 0;
}

/* Returns 0 on success, <0 else */
int
usocklnd_add_pollrequest(usock_conn_t *conn, int type, short value)
{
        int                  pt_idx = conn->uc_pt_idx;
        usock_pollthread_t  *pt     = &usock_data.ud_pollthreads[pt_idx];
        usock_pollrequest_t *pr;

        LIBCFS_ALLOC(pr, sizeof(*pr));
        if (pr == NULL) {
                CERROR ("Cannot allocate poll request\n");
                return -ENOMEM;
        }

        pr->upr_conn = conn;
        pr->upr_type = type;
        pr->upr_value = value;

        usocklnd_conn_addref(conn); /* +1 for poll request */
        
        pthread_mutex_lock(&pt->upt_pollrequests_lock);

        if (pt->upt_errno) { /* very rare case: errored poll thread */
                int rc = pt->upt_errno;
                pthread_mutex_unlock(&pt->upt_pollrequests_lock);
                usocklnd_conn_decref(conn);
                LIBCFS_FREE(pr, sizeof(*pr));
                return rc;
        }
        
        list_add_tail(&pr->upr_list, &pt->upt_pollrequests);
        pthread_mutex_unlock(&pt->upt_pollrequests_lock);
        return 0;
}

void
usocklnd_add_killrequest(usock_conn_t *conn)
{
        int                  pt_idx = conn->uc_pt_idx;
        usock_pollthread_t  *pt     = &usock_data.ud_pollthreads[pt_idx];
        usock_pollrequest_t *pr     = conn->uc_preq;
        
        /* Use preallocated poll request because there is no good
         * workaround for ENOMEM error while killing connection */
        if (pr) {
                pr->upr_conn  = conn;
                pr->upr_type  = POLL_DEL_REQUEST;
                pr->upr_value = 0;

                usocklnd_conn_addref(conn); /* +1 for poll request */
                
                pthread_mutex_lock(&pt->upt_pollrequests_lock);
                
                if (pt->upt_errno) { /* very rare case: errored poll thread */
                        pthread_mutex_unlock(&pt->upt_pollrequests_lock);
                        usocklnd_conn_decref(conn);
                        return; /* conn will be killed in poll thread anyway */
                }

                list_add_tail(&pr->upr_list, &pt->upt_pollrequests);
                pthread_mutex_unlock(&pt->upt_pollrequests_lock);

                conn->uc_preq = NULL;
        }
}

#if defined(__sun__) || defined(__sun)
#define USOCK_DP_ADD_REQ(old_value, new_value)                        \
do {                                                                  \
        if (new_value != old_value) {                                 \
                /* 'value' is our intention but kernel implementation \
                 * of /dev/poll will do bitwise 'OR' */               \
                if ((old_value | new_value) != new_value) {           \
                        rc = usocklnd_add_devpollrequest(pt_data,     \
                                                         conn->uc_lx->lx_fd, \
                                                         POLLREMOVE); \
                        if ( rc != 0 )                                \
                                goto process_pollrequest_error;       \
                }                                                     \
                                                                      \
                rc = usocklnd_add_devpollrequest(pt_data,             \
                                                 conn->uc_lx->lx_fd,  \
                                                 new_value);          \
                if ( rc != 0 )                                        \
                        goto process_pollrequest_error;               \
        }                                                             \
} while (0)        

#define USOCK_DP_REM_REQ                                              \
do {                                                                  \
        rc = usocklnd_add_devpollrequest(pt_data, conn->uc_lx->lx_fd, \
                                         POLLREMOVE);                 \
        if ( rc != 0 )                                                \
                goto process_pollrequest_error;                       \
} while (0)
#else
#define USOCK_DP_ADD_REQ(old_value, new_value) do { } while (0)
#define USOCK_DP_REM_REQ                       do { } while (0)
#endif

/* Process poll request. Update poll data.
 * Returns 0 on success, <0 else */
int
usocklnd_process_pollrequest(usock_pollrequest_t *pr,
                             usock_pollthread_t *pt_data)
{
        int            type  = pr->upr_type;
        short          value = pr->upr_value;
        usock_conn_t  *conn  = pr->upr_conn;
        struct lnet_xport *lx;

        int            idx = 0;

        struct pollfd *pollfd   = pt_data->upt_pollfd;
        int           *fd2idx   = pt_data->upt_fd2idx;
        usock_conn_t **idx2conn = pt_data->upt_idx2conn;
        int           *skip     = pt_data->upt_skip;
#if defined(__sun__) || defined(__sun)
        struct pollfd *requests = pt_data->upt_requests;
        struct pollfd *results  = pt_data->upt_results;
        int            rc       = -ENOMEM; /* default */
#endif
        
        LASSERT(conn != NULL);

	/*
	 * If two events get queued, and the first one is a DEL, our
	 * transport structure can disappear.  In this situation, simply
	 * release our connection ref.
	 */
	if (conn->uc_lx == NULL) {
		if (type == POLL_ADD_REQUEST ||
		    type == POLL_DEL_REQUEST)
			goto out;
		goto skip;
	}

        LASSERT(conn->uc_lx->lx_fd >=0);
        LASSERT(type == POLL_ADD_REQUEST ||
                conn->uc_lx->lx_fd < pt_data->upt_nfd2idx);

        if (type != POLL_ADD_REQUEST) {
                idx = fd2idx[conn->uc_lx->lx_fd];
                if (idx > 0 && idx < pt_data->upt_nfds) { /* hot path */
                        LASSERT(pollfd[idx].fd == conn->uc_lx->lx_fd);
                } else { /* unlikely */
                        CWARN("Very unlikely event happend: trying to"
                              " handle poll request of type %d but idx=%d"
                              " is out of range [1 ... %d]. Is shutdown"
                              " in progress (%d)?\n",
                              type, idx, pt_data->upt_nfds - 1,
                              usock_data.ud_shutdown);
 out:
                        LIBCFS_FREE (pr, sizeof(*pr));
                        usocklnd_conn_decref(conn);
                        return 0;
                }
        }

 skip:
        LIBCFS_FREE (pr, sizeof(*pr));
        
        switch (type) {
        case POLL_ADD_REQUEST:
                if (pt_data->upt_nfds >= pt_data->upt_npollfd) {
                        /* resize pollfd[], idx2conn[] and skip[] */
                        struct pollfd *new_pollfd;
                        int            new_npollfd = pt_data->upt_npollfd * 2;
                        usock_conn_t **new_idx2conn;
                        int           *new_skip;
#if defined(__sun__) || defined(__sun)
                        struct pollfd *new_requests;
                        struct pollfd *new_results;
#endif

                        new_pollfd = LIBCFS_REALLOC(pollfd, new_npollfd *
                                                     sizeof(struct pollfd));
                        if (new_pollfd == NULL)
                                goto process_pollrequest_error;
                        pt_data->upt_pollfd = pollfd = new_pollfd;
                        
                        new_idx2conn = LIBCFS_REALLOC(idx2conn, new_npollfd *
                                                      sizeof(usock_conn_t *));
                        if (new_idx2conn == NULL)
                                goto process_pollrequest_error;
                        pt_data->upt_idx2conn = idx2conn = new_idx2conn;

                        new_skip = LIBCFS_REALLOC(skip, new_npollfd *
                                                  sizeof(int));
                        if (new_skip == NULL)
                                goto process_pollrequest_error;
                        pt_data->upt_skip = new_skip;

#if defined(__sun__) || defined(__sun)
                        new_requests = LIBCFS_REALLOC(requests, new_npollfd *
                                                      sizeof(struct pollfd));
                        if (new_requests == NULL)
                                goto process_pollrequest_error;
                        pt_data->upt_requests = requests = new_requests;

                        new_results = LIBCFS_REALLOC(results, new_npollfd *
                                                      sizeof(struct pollfd));
                        if (new_results == NULL)
                                goto process_pollrequest_error;
                        pt_data->upt_results = results = new_results;
#endif
                        
                        pt_data->upt_npollfd = new_npollfd;
                }

                if (conn->uc_lx->lx_fd >= pt_data->upt_nfd2idx) {
                        /* resize fd2idx[] */
                        int *new_fd2idx;
                        int  new_nfd2idx = pt_data->upt_nfd2idx * 2;

                        while (new_nfd2idx <= conn->uc_lx->lx_fd)
                                new_nfd2idx *= 2;

                        new_fd2idx = LIBCFS_REALLOC(fd2idx, new_nfd2idx *
                                                    sizeof(int));
                        if (new_fd2idx == NULL)
                                goto process_pollrequest_error;

                        pt_data->upt_fd2idx = fd2idx = new_fd2idx;
                        memset(fd2idx + pt_data->upt_nfd2idx, 0,
                               (new_nfd2idx - pt_data->upt_nfd2idx)
                               * sizeof(int));
                        pt_data->upt_nfd2idx = new_nfd2idx;
                }

                LASSERT(fd2idx[conn->uc_lx->lx_fd] == 0);

                idx = pt_data->upt_nfds++;
                idx2conn[idx] = conn;
                fd2idx[conn->uc_lx->lx_fd] = idx;

                pollfd[idx].fd = conn->uc_lx->lx_fd;
                pollfd[idx].events = value;
                pollfd[idx].revents = 0;
                
                USOCK_DP_ADD_REQ(0, value);
                break;
        case POLL_DEL_REQUEST:
                fd2idx[conn->uc_lx->lx_fd] = 0; /* invalidate this entry */
                
                --pt_data->upt_nfds;
                if (idx != pt_data->upt_nfds) {
                        /* shift last entry into released position */
                        memcpy(&pollfd[idx], &pollfd[pt_data->upt_nfds],
                               sizeof(struct pollfd));
                        idx2conn[idx] = idx2conn[pt_data->upt_nfds];
                        fd2idx[pollfd[idx].fd] = idx;                        
                }

                lx = conn->uc_lx;
                conn->uc_lx = NULL;
                lx_close(lx);
                close(lx->lx_fd);
                lx_destroy(lx); 

                list_add_tail(&conn->uc_stale_list, &pt_data->upt_stale_list);
                USOCK_DP_REM_REQ;
                break;
        case POLL_RX_SET_REQUEST:
                value |= pollfd[idx].events & ~POLLIN;
                USOCK_DP_ADD_REQ(pollfd[idx].events, value);
                pollfd[idx].events = value;
                break;
        case POLL_TX_SET_REQUEST:
                value |= pollfd[idx].events & ~POLLOUT;
                USOCK_DP_ADD_REQ(pollfd[idx].events, value);
                pollfd[idx].events = value;
                break;
        case POLL_SET_REQUEST:
                USOCK_DP_ADD_REQ(pollfd[idx].events, value);
                pollfd[idx].events = value;
                break;
        default:
                LBUG(); /* unknown type */                
        }

        /* In the case of POLL_ADD_REQUEST, idx2conn[idx] takes the
         * reference that poll request possesses */
        if (type != POLL_ADD_REQUEST)
                usocklnd_conn_decref(conn);
        
        return 0;

  process_pollrequest_error:
        usocklnd_conn_decref(conn);
        return -ENOMEM;
}

/* Loop on poll data executing handlers repeatedly until
 *  fair_limit is reached or all entries are exhausted */
void
usocklnd_execute_handlers(usock_pollthread_t *pt_data)
{
#if defined(__sun__) || defined(__sun)
        struct pollfd *pollfd      = pt_data->upt_results;
        int            nfds        = pt_data->upt_nresults;
        int            notifier_fd = pt_data->upt_pollfd[0].fd;
        int            ntfd_skp_fl = 0; /* 0 means "not skipped yet" */
        int           *fd2idx      = pt_data->upt_fd2idx;
        int            fd;
        int            idx;
#else
        struct pollfd *pollfd      = pt_data->upt_pollfd;
        int            nfds        = pt_data->upt_nfds;
#endif        
        usock_conn_t **idx2conn    = pt_data->upt_idx2conn;
        int           *skip        = pt_data->upt_skip;
        int            j;

#if defined(__sun__) || defined(__sun)
        skip[0] = 0; /* we don't know whether results[] has notifier fd */
#else
        if (pollfd[0].revents & POLLIN)
                while (usocklnd_notifier_handler(pollfd[0].fd) > 0)
                        ;

        skip[0] = 1; /* always skip notifier fd */
#endif

        for (j = 0; j < usock_tuns.ut_fair_limit; j++) {
                int prev = 0;
                int i = skip[0];
                
                if (i >= nfds) /* nothing ready */
                        break;
                
                do {
                        usock_conn_t *conn;
                        int next;
                        
                        if (j == 0) /* first pass... */
                                next = skip[i] = i+1; /* set skip chain */
                        else /* later passes... */
                                next = skip[i]; /* skip unready pollfds */

#if defined(__sun__) || defined(__sun)
                        fd = pollfd[i].fd;
                        LASSERT(fd >= 0 && fd < pt_data->upt_nfd2idx);

                        idx = fd2idx[fd];
                        LASSERT(idx >= 0 && idx < pt_data->upt_nfds);

                        conn = idx2conn[idx];
                        LASSERT(idx == 0 || conn != NULL);
                        LASSERT(idx == 0 || conn->uc_lx->lx_fd == fd);
                                                                
                        if (idx == 0) { /* fd is notifier */
                                LASSERT(fd == notifier_fd);
                                LASSERT(ntfd_skp_fl++ == 0);
                                
                                LASSERT(pollfd[0].revents & POLLIN);
                                while (usocklnd_notifier_handler(notifier_fd) > 0)
                                        ;

                                skip[prev] = next; /* skip notifier_fd */
                                i = next;
                                continue;
                        }
#else
                        conn = idx2conn[i];
#endif
                        
                        /* kill connection if it's closed by peer and
                         * there is no data pending for reading */
                        if ((pollfd[i].revents & POLLERR) != 0 ||
                            (pollfd[i].revents & POLLHUP) != 0) {
                                if ((pollfd[i].events & POLLIN) != 0 &&
                                    (pollfd[i].revents & POLLIN) == 0)
                                        usocklnd_conn_kill(conn);
                                else
                                        usocklnd_exception_handler(conn);
                        }
                        
                        if ((pollfd[i].revents & POLLIN) != 0 &&
                            usocklnd_read_handler(conn) <= 0)
                                pollfd[i].revents &= ~POLLIN;
                        
                        if ((pollfd[i].revents & POLLOUT) != 0 &&
                            usocklnd_write_handler(conn) <= 0)
                                pollfd[i].revents &= ~POLLOUT;
                        
                        if ((pollfd[i].revents & (POLLIN | POLLOUT)) == 0)
                                skip[prev] = next; /* skip this entry next pass */
                        else
                                prev = i;
                        
                        i = next;
                } while (i < nfds);
        }
}

int
usocklnd_calculate_chunk_size(int num)
{
        const int n     = 4;
        const int p     = usock_tuns.ut_poll_timeout;
        int       chunk = num;
        
        /* chunk should be big enough to detect a timeout on any
         * connection within (n+1)/n times the timeout interval
         * if we checks every 'p' seconds 'chunk' conns */
                 
        if (usock_tuns.ut_timeout > n * p)
                chunk = (chunk * n * p) / usock_tuns.ut_timeout;
        
        if (chunk == 0)
                chunk = 1;

        return chunk;
}

void
usocklnd_wakeup_pollthread(int i)
{
        usock_pollthread_t *pt = &usock_data.ud_pollthreads[i];
        int                 notification = 0;
        int                 rc;

        rc = syscall(SYS_write, pt->upt_notifier_fd, &notification,
                     sizeof(notification));

        if (rc != sizeof(notification))
                CERROR("Very unlikely event happend: "
                       "cannot write to notifier fd (rc=%d; errno=%d)\n",
                       rc, errno);
}
