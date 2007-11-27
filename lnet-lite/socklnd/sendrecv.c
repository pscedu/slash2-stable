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

int
zest_sock_write (int sock, void *buffer, int nob, int timeout)
{
        int            rc;
        struct timeval tv, then;
        
        LASSERT (nob > 0);
        /* Caller may pass a zero timeout if she thinks the socket buffer is        
         * empty enough to take the whole message immediately */

        gettimeofday(&then, NULL);

        for (;;) {
                struct iovec  iov = {
                        .iov_base = buffer,
                        .iov_len  = nob
                };
                struct msghdr msg = {
                        .msg_name       = NULL,
                        .msg_namelen    = 0,
                        .msg_iov        = &iov,
                        .msg_iovlen     = 1,
                        .msg_control    = NULL,
                        .msg_controllen = 0,
                        .msg_flags      = (timeout == 0) ? MSG_DONTWAIT : 0
                };
                
                if (timeout != 0) {
                        /* Set send timeout to remaining time */
                        tv = (struct timeval) {
                                .tv_sec = timeout,
                                .tv_usec = 0
                        };
                        rc = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                                        (char *)&tv, sizeof(tv));
                        if (rc != 0) {
                                CERROR("Can't set socket send timeout "
                                       "%ld.%06d: %d\n",
                                       (long)tv.tv_sec, (int)tv.tv_usec, rc);
                                return rc;
                        }
                }
                
                rc = sendmsg (sock, &msg, iov.iov_len);

                gettimeofday(&tv, NULL);

                if (rc == nob)
                        return 0;
                
                if (rc < 0)
                        return rc;
                
                if (rc == 0) {
                        CERROR ("Unexpected zero rc\n");
                        return (-ECONNABORTED);
                }
                
                if (tv.tv_sec - then.tv_sec > timeout)
                        return -EAGAIN;
                
                buffer = ((char *)buffer) + rc;
                nob -= rc;
        }
        
        return (0);
}

int
zest_sock_read (int sock, void *buffer, int nob, int timeout)
{
        int            rc;
        struct timeval tv, then;
        
        LASSERT (nob > 0);
        LASSERT (timeout > 0);

        gettimeofday(&then, NULL);

        for (;;) {
                struct iovec  iov = {
                        .iov_base = buffer,
                        .iov_len  = nob
                };
                struct msghdr msg = {
                        .msg_name       = NULL,
                        .msg_namelen    = 0,
                        .msg_iov        = &iov,
                        .msg_iovlen     = 1,
                        .msg_control    = NULL,
                        .msg_controllen = 0,
                        .msg_flags      = 0
                };
                
                if (timeout != 0) {
                        /* Set send timeout to remaining time */
                        tv = (struct timeval) {
                                .tv_sec = timeout,
                                .tv_usec = 0
                        };
                        rc = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                                        (char *)&tv, sizeof(tv));
                        if (rc != 0) {
                                CERROR("Can't set socket send timeout "
                                       "%ld.%06d: %d\n",
                                       (long)tv.tv_sec, (int)tv.tv_usec, rc);
                                return rc;
                        }
                }
                
                rc = recvmsg (sock, &msg, 0);

                gettimeofday(&tv, NULL);

                if (rc < 0)
                        return rc;

                if (rc == 0)
                        return -ECONNRESET;

                buffer = ((char *)buffer) + rc;
                nob -= rc;

                if (nob == 0)
                        return 0;

                if (tv.tv_sec - then.tv_sec > timeout)
                        return -ETIMEDOUT;
        }        
        return (0);
}
