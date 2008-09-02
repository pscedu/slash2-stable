/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-  
 * vim:expandtab:shiftwidth=8:tabstop=8:      
 */                                                                   
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <lnet/types.h>
#include <lnet/lib-types.h>
#include <libcfs/kp30.h>
#include <errno.h>

extern int tcpnal_maxsendkb;

int
psc_sock_io (int sock, void *buffer, int nob, int timeout, int rw)
{
        int            rc;
        struct timeval tv, then;

        LASSERT(nob > 0);
        /* Caller may pass a zero timeout if she thinks the socket buffer is
         * empty enough to take the whole message immediately */
        if (rw)
                LASSERT(tcpnal_maxsendkb);
        else
                LASSERT(timeout > 0);
        
        gettimeofday(&then, NULL);

        for (;;) {
                struct iovec  iov = {
                        .iov_base = buffer,
                        .iov_len  = MIN(nob, tcpnal_maxsendkb)
                };
                struct msghdr msg = {
                        .msg_name       = NULL,
                        .msg_namelen    = 0,
                        .msg_iov        = &iov,
                        .msg_iovlen     = 1,
                        .msg_control    = NULL,
                        .msg_controllen = 0,
                        .msg_flags      = (rw && !timeout) ? MSG_DONTWAIT : 0
                };
                
                if (timeout != 0) {
                        /* Set send timeout to remaining time */
                        tv = (struct timeval) {
                                .tv_sec = timeout,
                                .tv_usec = 0
                        };
                        rc = setsockopt(sock, SOL_SOCKET,
                                        (rw ? SO_SNDTIMEO : SO_RCVTIMEO),
                                        (char *)&tv, sizeof(tv));
                        if (rc != 0) {
                                CERROR("Can't set socket %s timeout "
                                       "%ld.%06d: %d\n",
                                       (rw ? "send" : "recv"),
                                       (long)tv.tv_sec, (int)tv.tv_usec, rc);
                                return -errno;
                        }
                }
                if (rw)
                        rc = sendmsg(sock, &msg, iov.iov_len);
                else
                        rc = recvmsg(sock, &msg, iov.iov_len);

                if (rc < 0)
                        return (-errno);

                if (rc == 0) {
                        CERROR ("Unexpected zero rc\n");
                        return (rw ? -ECONNABORTED : -ECONNRESET);
                }
                nob -= rc;
                if (!nob)
                        return (0);
                               
                gettimeofday(&tv, NULL);
                if (tv.tv_sec - then.tv_sec > timeout)
                        return (rw ? -EAGAIN : -ETIMEDOUT);

                buffer = ((char *)buffer) + rc;
        }        
        return (0);
}
