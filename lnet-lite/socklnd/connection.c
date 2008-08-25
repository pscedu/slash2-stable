/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2002 Cray Inc.
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

/* connection.c:
   This file provides a simple stateful connection manager which
   builds tcp connections on demand and leaves them open for
   future use. 
*/

#include <stdlib.h>
#include <pqtimer.h>
#include <dispatch.h>
#include <table.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <lnet/types.h>
#include <lnet/lib-types.h>
#include <lnet/socklnd.h>
#include <libcfs/kp30.h>
#include <connection.h>
#include <pthread.h>
#include <errno.h>
#if defined (__APPLE__)
#include <sys/syscall.h>
#elif !defined(__CYGWIN__)
#include <syscall.h>
#endif

#include "sendrecv.h"

#include <sdp_inet.h>
int tcpnal_usesdp = 0;

/* tunables (via environment) */
int tcpnal_acceptor_port = 988;
int tcpnal_connector_port = 988;
int tcpnal_buffer_size   = 0;
int tcpnal_nagle         = 0;
int tcpnal_server        = 0;
int tcpnal_maxsendkb	 = 1024;
int tcpnal_portinc	 = 0;

#define HELLO_TIMEOUT 5

int
tcpnal_env_param (char *name, int *val)
{
        char   *env = getenv(name);
        int     n;

        if (env == NULL)
                return 1;

        n = strlen(env);                /* scanf may not assign on EOS */
        if (sscanf(env, "%i%n", val, &n) >= 1 && n == (int)strlen(env)) {
                CDEBUG(D_INFO, "Environment variable %s set to %d\n",
                       name, *val);
                return 1;
        }

        CERROR("Can't parse environment variable '%s=%s'\n",
               name, env);
        return 0;
}

int
tcpnal_set_global_params (void)
{
        return  tcpnal_env_param("TCPNAL_PORT",
                                &tcpnal_acceptor_port) &&
                tcpnal_env_param("TCPLND_USESDP",
                                 &tcpnal_usesdp)       &&
                tcpnal_env_param("TCPLND_PORT",
                                &tcpnal_acceptor_port) &&
		/* Default to same port as acceptor. */
		(tcpnal_connector_port = tcpnal_acceptor_port, 1) &&
                tcpnal_env_param("TCPLND_CPORT",
                                &tcpnal_connector_port) &&
                tcpnal_env_param("TCPLND_BUFFER_SIZE",
                                 &tcpnal_buffer_size) &&
                tcpnal_env_param("TCPNAL_NAGLE",
                                 &tcpnal_nagle) &&
                tcpnal_env_param("TCPLND_NAGLE",
                                 &tcpnal_nagle) &&
                tcpnal_env_param("TCPLND_SERVER",
                                 &tcpnal_server);
                tcpnal_env_param("TCPLND_MAXSENDKB",
                                 &tcpnal_maxsendkb);
                tcpnal_env_param("TCPLND_PORTINC",
                                 &tcpnal_portinc);
}

/* Function:  compare_connection
 * Arguments: connection c:      a connection in the hash table
 *            lnet_process_id_t:  an id to verify  agains
 * Returns: 1 if the connection is the one requested, 0 otherwise
 *
 *    compare_connection() tests for collisions in the hash table
 */
static int 
compare_connection(void *arg1, void *arg2)
{
    connection  c = arg1;
    lnet_process_id_t *nidpid = arg2;

    return ((c->peer_nid == nidpid->nid) &&
            (c->peer_pid == nidpid->pid));
}

/* Function:  connection_key
 * Arguments: lnet_process_id_t id:  an id to hash
 * Returns: a not-particularily-well-distributed hash
 *          of the id
 */
static unsigned int 
connection_key(void *arg)
{
        lnet_nid_t *nid = arg;
        
        return (unsigned int)(*nid);
}

void
close_connection(void *arg)
{
        connection c = arg;
        int rc;
                       
        rc = shutdown(c->fd, SHUT_RDWR);
        if (rc) 
                CERROR("shutdown of sock %d (nid %s, pid %u) failed: %s\n",
                       c->fd, libcfs_nid2str(c->peer_nid), c->peer_pid,
                       strerror(errno));
        close(c->fd);

        CDEBUG(D_NET, "freeing conn %p", c);
        free(c);
}

/* Function:  remove_connection
 * Arguments: c: the connection to remove
 */
void 
remove_connection(void *arg)
{
        connection c = arg;

        CDEBUG(D_NET, "Removing conn (nid %s, pid %u)\n",
               libcfs_nid2str(c->peer_nid), c->peer_pid);
        
        hash_table_remove(c->m->connections,&c->peer_nid);
        remove_io_handler(c->conn_iohandler);
        close_connection(c);
}


/* Function:  read_connection: 
 * Arguments: c:    the connection to read from 
 *            dest: the buffer to read into
 *            len:  the number of bytes to read   
 * Returns: success as 1, or failure as 0
 *
 *   read_connection() reads data from the connection, continuing
 *   to read partial results until the request is satisfied or
 *   it errors. TODO: this read should be covered by signal protection.
 */
int 
read_connection(connection c, unsigned char *dest, int len)
{
    int offset = 0,rc;

    CDEBUG(D_NET, "c addr %p", c);

    if (len) {
            do {
#ifndef __CYGWIN__
                    rc = syscall(SYS_read, c->fd, dest+offset, len-offset);
#else
                    rc = recv(c->fd, dest+offset, len-offset, 0);
#endif
                    if (rc <= 0) {
                            if (errno == EINTR) {
                                    rc = 0;
                            } else {
                                    CDEBUG(D_WARNING, 
                                           "recverr rc=%d sock=%d(nid %s," 
                                           " pid %u):%s\n",
                                           rc, c->fd, 
                                           libcfs_nid2str(c->peer_nid),
                                           c->peer_pid, strerror(errno));
                                    remove_connection(c);
                                    return (0);
                            }
                    }
                    offset += rc;
            } while (offset < len);
    }
    return (1);
}

static int connection_input(void *d)
{
        connection c = d;
        return((*c->m->handler)(c->m->handler_arg,c));
}


static connection 
allocate_connection(manager m, lnet_process_id_t *nidpid, int fd)
{
    bridge     b = m->handler_arg;
    connection c = malloc(sizeof(struct connection));

    LASSERT(c != NULL);

    c->m  = m;
    c->fd = fd;
    c->peer_nid = nidpid->nid;
    c->peer_pid = nidpid->pid;

    c->conn_iohandler = register_io_handler(fd, READ_HANDLER, 
                                            &b->b_io_handler,
                                            connection_input, c);

    CDEBUG(D_NET, "Adding connection (%p) to nid %u-%s\n",
           c, nidpid->pid, libcfs_nid2str(nidpid->nid));

    hash_table_insert(m->connections, c, nidpid);
    return(c);
}

int
tcpnal_write(lnet_nid_t nid, int sockfd, void *buffer, int nob)
{
        int rc = syscall(SYS_write, sockfd, buffer, nob);
        
        /* NB called on an 'empty' socket with huge buffering! */
        if (rc == nob)
                return 0;

        if (rc < 0) {
                CERROR("Failed to send to %s: %s\n",
                       libcfs_nid2str(nid), strerror(errno));
                return -1;
        }
        
        CERROR("Short send to %s: %d/%d\n",
               libcfs_nid2str(nid), rc, nob);
        return -1;
}

int
tcpnal_read(lnet_nid_t nid, int sockfd, void *buffer, int nob) 
{
        int       rc;

        while (nob > 0) {
                rc = syscall(SYS_read, sockfd, buffer, nob);
                
                if (rc == 0) {
                        CERROR("Unexpected EOF from %s\n",
                               libcfs_nid2str(nid));
                        return -1;
                }

                if (rc < 0) {
                        CERROR("Failed to receive from %s: %s\n",
                               libcfs_nid2str(nid), strerror(errno));
                        return -1;
                }

                nob -= rc;
        }
        return 0;
}



int
tcpnal_hello (int sockfd, lnet_nid_t nid)
{
        struct timeval          tv;
        __u64                   incarnation;
        int                     rc;
        int                     nob;
        lnet_acceptor_connreq_t cr;
        lnet_hdr_t              hdr;
        lnet_magicversion_t     hmv;
        lnet_process_id_t       id;

        LNetGetId(1, &id);

        CDEBUG(D_NET, "saying hello to %s, (i am %s %u)\n",
               libcfs_nid2str(nid), libcfs_nid2str(id.nid),
               id.pid);

        gettimeofday(&tv, NULL);
        incarnation = (((__u64)tv.tv_sec) * 1000000) + tv.tv_usec;

        memset(&cr, 0, sizeof(cr));
        cr.acr_magic   = LNET_PROTO_ACCEPTOR_MAGIC;
        cr.acr_version = LNET_PROTO_ACCEPTOR_VERSION;
        cr.acr_nid     = nid;

        /* hmv initialised and copied separately into hdr; compiler "optimize"
         * likely due to confusion about pointer alias of hmv and hdr when this
         * was done in-place. */
        hmv.magic         = cpu_to_le32(LNET_PROTO_TCP_MAGIC);
        hmv.version_major = cpu_to_le32(LNET_PROTO_TCP_VERSION_MAJOR);
        hmv.version_minor = cpu_to_le32(LNET_PROTO_TCP_VERSION_MINOR);

        memset (&hdr, 0, sizeof (hdr));

        CLASSERT (sizeof (hmv) == sizeof (hdr.dest_nid));
        memcpy(&hdr.dest_nid, &hmv, sizeof(hmv));

        /* hdr.src_nid/src_pid are ignored at dest */

        hdr.type    = cpu_to_le32(LNET_MSG_HELLO);
        hdr.msg.hello.type = cpu_to_le32(SOCKLND_CONN_ANY);
        hdr.msg.hello.incarnation = cpu_to_le64(incarnation);

        hdr.src_nid = id.nid;
        hdr.src_pid = id.pid;
        
        /* I don't send any interface info */

        /* Assume sufficient socket buffering for these messages... */
        rc = tcpnal_write(nid, sockfd, &cr, sizeof(cr));
        if (rc != 0)
                return -1;

        rc = tcpnal_write(nid, sockfd, &hdr, sizeof(hdr));
        if (rc != 0)
                return -1;

        rc = tcpnal_read(nid, sockfd, &hmv, sizeof(hmv));
        if (rc != 0)
                return -1;
        
        if (hmv.magic != le32_to_cpu(LNET_PROTO_TCP_MAGIC)) {
                CERROR ("Bad magic %#08x (%#08x expected) from %s\n",
                        cpu_to_le32(hmv.magic), LNET_PROTO_TCP_MAGIC, 
                        libcfs_nid2str(nid));
                return -1;
        }

        if (hmv.version_major != cpu_to_le16 (LNET_PROTO_TCP_VERSION_MAJOR) ||
            hmv.version_minor != cpu_to_le16 (LNET_PROTO_TCP_VERSION_MINOR)) {
                CERROR ("Incompatible protocol version %d.%d (%d.%d expected)"
                        " from %s\n",
                        le16_to_cpu (hmv.version_major),
                        le16_to_cpu (hmv.version_minor),
                        LNET_PROTO_TCP_VERSION_MAJOR,
                        LNET_PROTO_TCP_VERSION_MINOR,
                        libcfs_nid2str(nid));
                return -1;
        }

#if (LNET_PROTO_TCP_VERSION_MAJOR != 1)
# error "This code only understands protocol version 1.x"
#endif
        /* version 1 sends magic/version as the dest_nid of a 'hello' header,
         * so read the rest of it in now... */

        rc = tcpnal_read(nid, sockfd, ((char *)&hdr) + sizeof (hmv),
                         sizeof(hdr) - sizeof(hmv));
        if (rc != 0)
                return -1;

        /* ...and check we got what we expected */
        if (hdr.type != cpu_to_le32 (LNET_MSG_HELLO)) {
                CERROR ("Expecting a HELLO hdr "
                        " but got type %d with %d payload from %s\n",
                        le32_to_cpu (hdr.type),
                        le32_to_cpu (hdr.payload_length), libcfs_nid2str(nid));
                return -1;
        }

        if (le64_to_cpu(hdr.src_nid) == LNET_NID_ANY) {
                CERROR("Expecting a HELLO hdr with a NID, but got LNET_NID_ANY\n");
                return -1;
        }

        if (nid != le64_to_cpu (hdr.src_nid)) {
                CERROR ("Connected to %s, but expecting %s\n",
                        libcfs_nid2str(le64_to_cpu (hdr.src_nid)), 
                        libcfs_nid2str(nid));
                return -1;
        }

        /* Ignore any interface info in the payload */
        nob = le32_to_cpu(hdr.payload_length);
        if (nob != 0) {
                CERROR("Unexpected HELLO payload %d from %s\n",
                       nob, libcfs_nid2str(nid));
                return -1;
        }

        return 0;
}

static int
tcpnal_invert_type(int type)
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
                        return (SOCKLND_CONN_NONE);
                }
}

int 
tcpnal_hello_handle_v1(int sock, lnet_process_id_t *nidpid, 
                       __unusedx int flipped) 
{
        lnet_magicversion_t *hmv;
        lnet_hdr_t           hdr;
        int                  len = sizeof(hdr);
        void                *buf = (void *)&hdr;
        lnet_process_id_t    id;

        id.nid = nidpid->nid;
        //LNetGetId(1, &id);        
        
        if (psc_sock_read (sock, buf, len, HELLO_TIMEOUT)) {
                CERROR("psc_sock_read() from %d failed: %s\n",
                       sock, strerror(errno));
                return -1;
        }
       
        CDEBUG(D_TRACE, 
               "New Conn:\n\tHdr.dest_nid = %s\n" 
               "\tHdr.src_nid  = %s\n"
               "\tHdr.dest_pid = %u\n"
               "\tHdr.src_pid  = %u\n"           
               "\tHdr.type     = %u\n"                      
               "\tHdr.payllen  = %u\n"           
               "\tHdr.hello.incarnation  = "LPX64"\n"
               "\tHdr.hello.type  = %u\n",           
               libcfs_nid2str(hdr.dest_nid),
               libcfs_nid2str(hdr.src_nid),
               hdr.dest_pid, 
               hdr.src_pid,
               hdr.type,
               hdr.payload_length,
               hdr.msg.hello.incarnation,
               hdr.msg.hello.type);        
        /*
         *  hand the nidpid back to the caller
         */
        nidpid->nid = hdr.src_nid;
        nidpid->pid = hdr.src_pid;
        /*
         * Encode the hello reply
         */
        hdr.src_nid = id.nid;
        hdr.src_pid = the_lnet.ln_pid;
        hdr.msg.hello.type = tcpnal_invert_type(hdr.msg.hello.type);
                
        hmv = (lnet_magicversion_t *)&hdr.dest_nid;        
        hmv->magic = LNET_PROTO_TCP_MAGIC;
        hmv->version_major = LNET_PROTO_TCP_VERSION_MAJOR;
        hmv->version_minor = LNET_PROTO_TCP_VERSION_MINOR;
        
        if (psc_sock_write (sock, buf, len, HELLO_TIMEOUT)) {
                CERROR("psc_sock_write() to %d of hello reply failed: %s\n",
                       sock, strerror(errno));
                return -1;
        }
        CDEBUG(D_NET, 
               "v1 Hello Reply:\n\tHdr.dest_nid = %s\n" 
               "\tHdr.src_nid  = %s\n"
               "\tHdr.dest_pid = %u\n"
               "\tHdr.src_pid  = %u\n"           
               "\tHdr.type     = %u\n"                      
               "\tHdr.payllen  = %u\n"           
               "\tHdr.hello.incarnation  = "LPX64"\n"
               "\tHdr.hello.type  = %u\n",           
               libcfs_nid2str(hdr.dest_nid),
               libcfs_nid2str(hdr.src_nid),
               hdr.dest_pid, 
               hdr.src_pid,
               hdr.type,
               hdr.payload_length,
               hdr.msg.hello.incarnation,
               hdr.msg.hello.type);        
        return 0;
}


/**
 * tcpnal_hello_handle_v2 - expect a v2 style packet but send a v1 reply 
 */
int 
tcpnal_hello_handle_v2(int sock, lnet_process_id_t *nidpid, 
                       __unusedx int flipped) 
{
        ksock_hello_msg_t    hello;
        lnet_hdr_t           reply;
        int                  len = (int)(sizeof(hello) - sizeof(hello.kshm_ips));
        void                *buf = (void *)&hello;
        lnet_process_id_t    id;
        __u64                incarnation;
        struct timeval       tv;
        lnet_magicversion_t *hmv;

        memset(&hello, 0, sizeof(hello));

        gettimeofday(&tv, NULL);
        incarnation = (((__u64)tv.tv_sec) * 1000000) + tv.tv_usec;

        id.nid = nidpid->nid;
        //LNetGetId(1, &id);

        if (psc_sock_read (sock, buf, len, HELLO_TIMEOUT)) {
                CERROR("psc_sock_read() from %d failed: %s\n",
                       sock, strerror(errno));
                return -1;
        }        

        CDEBUG(D_TRACE, 
               "New Conn v2 from (%d bytes)\n\thello.kshm_magic   = %u\n" 
               "\thello.kshm_version = %u\n"
               "\thello.kshm_src_nid = %s\n"           
               "\thello.kshm_dst_nid = %s\n"           
               "\thello.kshm_src_pid = %u\n"           
               "\thello.kshm_dst_pid = %u\n"           
               "\thello.kshm_src_incarnation = "LPX64"\n" 
               "\thello.kshm_dst_incarnation = "LPX64"\n" 
               "\thello.kshm_ctype = %u\n" 
               "\thello.kshm_nips = %u\n",
               len,
               hello.kshm_magic,
               hello.kshm_version,
               libcfs_nid2str(hello.kshm_src_nid),
               libcfs_nid2str(hello.kshm_dst_nid),
               hello.kshm_src_pid,
               hello.kshm_dst_pid,
               hello.kshm_src_incarnation,
               hello.kshm_dst_incarnation,
               hello.kshm_ctype,
               hello.kshm_nips);

        if (hello.kshm_magic != LNET_PROTO_MAGIC) { 
                CERROR("bad magic from peer %x should be %x\n", 
                       hello.kshm_magic, LNET_PROTO_MAGIC);
                return -EPROTO;
        }

        if (hello.kshm_nips != 0) {
                int kshm_ips[hello.kshm_nips];
                unsigned int i;

                if (psc_sock_read(sock, &kshm_ips,
                                   hello.kshm_nips * sizeof(__u32), 
                                   HELLO_TIMEOUT)) { 
                        CERROR ("Error reading IPs from nid %s\n",
                                libcfs_nid2str(hello.kshm_src_nid));
                        return -1;
                }

                for (i=0; i < hello.kshm_nips; i++) {
                        if (hello.kshm_ips[i] == 0) {
                                CERROR("Zero IP[%d]\n", i);
                                return -EPROTO;
                        } else 
                                CERROR("Got IP[%d] from ip %u\n",
                                       i, hello.kshm_ips[i]);
                }
        }
        /*
         *  hand the nidpid back to the caller
         */
        nidpid->nid = hello.kshm_src_nid;
        nidpid->pid = hello.kshm_src_pid;

        hmv = (lnet_magicversion_t *)&reply.dest_nid;
        
        hmv->magic = LNET_PROTO_TCP_MAGIC;
        hmv->version_major = LNET_PROTO_TCP_VERSION_MAJOR;
        hmv->version_minor = LNET_PROTO_TCP_VERSION_MINOR;

        reply.dest_pid       = hello.kshm_src_pid;
        reply.src_nid        = id.nid;
        reply.src_pid        = the_lnet.ln_pid;
        reply.type           = LNET_MSG_HELLO;
        reply.payload_length = 0;
        reply.msg.hello.incarnation = incarnation;
        reply.msg.hello.type        = tcpnal_invert_type(hello.kshm_ctype);

        if (psc_sock_write (sock, (void *)&reply, 
                             (int)sizeof(lnet_hdr_t), HELLO_TIMEOUT)) {
                CERROR("psc_sock_write() to %d of hello reply failed: %s\n",
                       sock, strerror(errno));
                return -1;
        }

        CDEBUG(D_TRACE, 
               "v2 Hello Reply:\n\tHdr.dest_nid = %s\n"
               "\tHdr.src_nid  = %s\n"
               "\tHdr.dest_pid = %u\n"
               "\tHdr.src_pid  = %u\n"
               "\tHdr.type     = %u\n"
               "\tHdr.payllen  = %u\n"
               "\tHdr.hello.incarnation  = "LPX64"\n"
               "\tHdr.hello.type  = %u\n",
               libcfs_nid2str(reply.dest_nid),
               libcfs_nid2str(reply.src_nid),
               reply.dest_pid,
               reply.src_pid,
               reply.type,
               reply.payload_length,
               reply.msg.hello.incarnation,
               reply.msg.hello.type);

        // this is bullshit v2 stuff i think
        ssize_t rc;
        unsigned long long b[128];
        memset(b, 0, 1024);
        
        rc = recv(sock, (unsigned char *)b, 24, 0);
        CERROR("got %zd extra bytes\n", rc);

        int i;
        for (i=0; i < 128; i+=4) { 
                CERROR("%016llx %016llx %016llx %016llx\n", 
                       (b[i]), 
                       (b[i+1]), 
                       (b[i+2]), 
                       (b[i+3]));
        }

        return 0;
}

/* Function:  force_tcp_connection
 * Arguments: t: tcpnal
 *            dest: portals endpoint for the connection
 * Returns: an allocated connection structure, either
 *          a pre-existing one, or a new connection
 */
connection 
force_tcp_connection(manager    m,
                     lnet_process_id_t *nidpid,
                     procbridge pb)
{
    unsigned int       ip = LNET_NIDADDR(nidpid->nid);
    connection         conn = NULL;
    bridge	       b = m->handler_arg;
    struct sockaddr_in addr;
    struct sockaddr_in locaddr; 
    int                fd;
    int                option;
    int                rc;
    socklen_t          sz;
    int                rport;

    pthread_mutex_lock(&m->conn_lock);

    CDEBUG(D_NET, "Looking up connection for %u-%s\n", 
           nidpid->pid, libcfs_nid2str(nidpid->nid));

    conn = hash_table_find(m->connections, &nidpid->nid);
    if (conn)
            goto out;

    CDEBUG(D_NET, "No conn found, establishing conn to %u-%s\n", 
           nidpid->pid, libcfs_nid2str(nidpid->nid)); 

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(ip);
    addr.sin_port        = htons(tcpnal_connector_port);

    memset(&locaddr, 0, sizeof(locaddr)); 
    locaddr.sin_family = AF_INET;
    locaddr.sin_addr.s_addr = INADDR_ANY;
    locaddr.sin_port = m->port;

    if (!tcpnal_server) { 
            fd = socket((tcpnal_usesdp ? AF_INET_SDP : AF_INET), SOCK_STREAM, 0);
            if (fd < 0) {
                    CERROR("tcpnal socket failed: %s\n", strerror(errno));
                    goto out;
            } 
            
            option = 1;
            rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                            &option, sizeof(option));
            if (rc != 0) {
                    CERROR ("Can't set SO_REUSEADDR for socket: %s\n", 
                            strerror(errno)); 
                    close(fd);
                    goto out;
            } 

            if (m->port != 0) {
                    /* Bind all subsequent connections to the same port */
                    rc = bind(fd, (struct sockaddr *)&locaddr, 
                              sizeof(locaddr));
                    if (rc != 0) {
                            CERROR("Error binding port: %s\n", 
                                   strerror(errno));
                            close(fd);
                            goto out;
                    }
            }

            rc = connect(fd, (struct sockaddr *)&addr,
                         sizeof(struct sockaddr_in));
            if (rc != 0) {
                    CERROR("Error connecting to remote host: %s\n", 
                           strerror(errno));
                    close(fd);
                    goto out;
            }
            
            sz = sizeof(locaddr);
            rc = getsockname(fd, (struct sockaddr *)&locaddr, &sz);
            if (rc != 0) {
                    CERROR("Error on getsockname: %s\n", strerror(errno));
                    close(fd);
                    goto out;
            }            

            if (m->port == 0)
                    m->port = ntohs(locaddr.sin_port);

    } else { 
            for (rport = IPPORT_RESERVED - 1; rport > IPPORT_RESERVED / 2; 
                 --rport) {
                    fd = socket((tcpnal_usesdp ? AF_INET_SDP : AF_INET), 
                                SOCK_STREAM, 0);
                    if (fd < 0) {
                            CERROR("tcpnal socket failed: %s\n", 
                                   strerror(errno));
                            goto out;
                    } 
                    
                    option = 1;
                    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                                    &option, sizeof(option));
                    if (rc != 0) {
                            CERROR("Can't set SO_REUSEADDR for socket: %s\n", 
                                   strerror(errno)); 
                            close(fd);
                            goto out;
                    } 
                    
                    locaddr.sin_port = htons(rport);
                    rc = bind(fd, (struct sockaddr *)&locaddr, 
                              sizeof(locaddr));

                    if (rc == 0 || errno == EACCES) {
                            rc = connect(fd, (struct sockaddr *)&addr,
                                         sizeof(struct sockaddr_in));
                            if (rc == 0) {
                                    break;
                            } else if (errno != EADDRINUSE && 
                                       errno != EADDRNOTAVAIL) {
                                    CERROR("Error connecting to remote host: %s\n", 
                                           strerror(errno));
                                    close(fd);
                                    goto out;
                            }
                    } else if (errno != EADDRINUSE) {
                            CERROR("Error binding to privileged port: %s\n", 
                                   strerror(errno));
                            close(fd);
                            goto out;
                    }
                    close(fd);
            }
            
            if (rport == IPPORT_RESERVED / 2) {
                    CERROR("Out of ports trying to bind to a reserved port\n");
                    goto out;
            }
    }
    
    option = tcpnal_nagle ? 0 : 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
    option = tcpnal_buffer_size;
    if (option != 0) {
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &option, sizeof(option));
            option = tcpnal_buffer_size;
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &option, sizeof(option));
    }
    
    /* say hello */
    if (tcpnal_hello(fd, nidpid->nid)) { 
            conn = NULL;
            goto out;            
    }
    
    conn = allocate_connection(m, nidpid, fd);
    
    /* let nal thread know this event right away */
    if (conn)
            procbridge_wakeup_nal(pb);

out:
    pthread_mutex_unlock(&m->conn_lock);
    return (conn);
}

connection 
force_tcp_connection_old(manager m, lnet_nid_t nid, procbridge pb)
{
        /* just spoof the lustre pid for now (12345) */
        lnet_process_id_t nidpid = {nid, 12345};
        return force_tcp_connection(m, &nidpid, pb);        
}
        
/* Function:  new_connection
 * Arguments: t: opaque argument holding the tcpname
 * Returns: 1 in order to reregister for new connection requests
 *
 *  called when the bound service socket recieves
 *     a new connection request, it always accepts and
 *     installs a new connection
 */
static int new_connection(void *z)
{
    manager            m = (manager)z;
    bridge             b = m->handler_arg;
    struct sockaddr_in s;
    lnet_process_id_t  nidpid = {b->b_ni->ni_nid, 0};
    int                fd;
    socklen_t          len = sizeof(struct sockaddr_in);
    unsigned int       ipaddr;
    ssize_t            rc;
    lnet_acceptor_connreq_t cr;
    ssize_t            cr_sz = sizeof(lnet_acceptor_connreq_t);
    int                lnet_magic;
    
    fd  = accept(m->bound,(struct sockaddr *)&s,&len);

    if (fd < 0) {
            CERROR("accept failed %s\n", strerror(errno));
            return 0;
    }
    ipaddr = *((unsigned int *)&s.sin_addr);

    rc = recv(fd, &cr, cr_sz, 0);
    
    if (rc != cr_sz) 
            goto failed_conn;

    CDEBUG(D_TRACE, "acr_magic %u, acr_version %u, acr_nid %s\n",
           cr.acr_magic, cr.acr_version, libcfs_nid2str(cr.acr_nid));

    if (cr.acr_magic != LNET_PROTO_ACCEPTOR_MAGIC) {            
            CERROR("recv'd invalid MAGIC from %d\n",
                   ipaddr);
            return 0;
    }
    
    if (cr.acr_version != LNET_PROTO_ACCEPTOR_VERSION) {
            CERROR("recv'd invalid VERSION %d from %d\n",
                   cr.acr_version, ipaddr);
            return 0;
    }
    /*
     * Grab the next 4 bytes, that will tell us if lustre is 
     *  working with v1 or v2 lnet
     */
    rc = recv(fd, &lnet_magic, sizeof(lnet_magic), MSG_PEEK);
    if (rc < 0 || rc != sizeof(lnet_magic))
            goto failed_conn;

    CDEBUG(D_TRACE, "Got 4 bytes for LNET_PROTO_MAGIC (%x) %x\n", 
           LNET_PROTO_MAGIC, lnet_magic);

    if (lnet_magic == LNET_PROTO_MAGIC)
            rc = tcpnal_hello_handle_v2(fd, &nidpid, 0);

    else 
            /* 
             * Assuming that this is the old hello
             */ 
            rc = tcpnal_hello_handle_v1(fd, &nidpid, 0);

    if (rc)
            goto failed_conn;

    CDEBUG(D_NET, "Conn from nid %s:0x%x\n",
           libcfs_nid2str(nidpid.nid), nidpid.pid);        

    pthread_mutex_lock(&m->conn_lock);
    allocate_connection(m, &nidpid, fd);
    pthread_mutex_unlock(&m->conn_lock);
    return(1);

 failed_conn:
    CERROR("Conn error %s\n", strerror(errno));
    return rc;
}

/* Function:  bind_socket
 * Arguments: t: the nal state for this interface
 *            port: the port to attempt to bind to
 * Returns: 1 on success, or 0 on error
 *
 * bind_socket() attempts to allocate and bind a socket to the requested
 *  port, or dynamically assign one from the kernel should the port be
 *  zero. Sets the bound and bound_handler elements of m.
 *
 *  TODO: The port should be an explicitly sized type.
 */
__unusedx static int 
bind_socket(manager m, unsigned short port)
{
        struct sockaddr_in addr;
        socklen_t alen=sizeof(struct sockaddr_in);
        bridge b=m->handler_arg;
        __unusedx unsigned int local_addr=LNET_NIDADDR(b->b_ni->ni_nid);
        int sock_type = (tcpnal_usesdp ? AF_INET_SDP : AF_INET);
	socklen_t option;

        if ((m->bound = socket(sock_type, SOCK_STREAM, 0)) < 0)  
                return(0);
        
        psc_warn("local bind address %x %x (mgr=%p) socknum=%d &b->b_io_handler %p", 
                 LNET_NIDADDR(b->b_ni->ni_nid), 
                 htonl(LNET_NIDADDR(b->b_ni->ni_nid)), m, m->bound, 
                 &b->b_io_handler);
        
        option = tcpnal_buffer_size;
        if (option != 0) {
                setsockopt(m->bound, SOL_SOCKET, SO_SNDBUF, &option, sizeof(option));
                option = tcpnal_buffer_size;
                setsockopt(m->bound, SOL_SOCKET, SO_RCVBUF, &option, sizeof(option));
        }
        
        bzero((char *) &addr, sizeof(addr));
        //addr.sin_family      = (tcpnal_usesdp ? AF_INET_SDP : AF_INET);
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(LNET_NIDADDR(b->b_ni->ni_nid));
        addr.sin_port        = htons(port);
        
        if (bind(m->bound,(struct sockaddr *)&addr,alen)<0){
                CERROR ("tcpnal bind: %s", strerror(errno)); 
                return(0);
        }
        psc_warn("local bind address %x %x (mgr=%p) "
                 "socknum=%d &b->b_io_handler %p", 
                 LNET_NIDADDR(b->b_ni->ni_nid), 
                 htonl(LNET_NIDADDR(b->b_ni->ni_nid)), m, m->bound, 
                 &b->b_io_handler);
        
        getsockname(m->bound,(struct sockaddr *)&addr, &alen);
        
        m->bound_handler=register_io_handler(m->bound,
                                             READ_HANDLER,
                                             &b->b_io_handler,
                                             new_connection, m);        
        listen(m->bound, 5); 
        m->port=addr.sin_port;
        return(1);
}


/* Function:  shutdown_connections
 * Arguments: m: the manager structure
 *
 * close all connections and reclaim resources
 */
void 
shutdown_connections(manager m)
{
        //#if 0
        /* we don't accept connections */
        close(m->bound);
        remove_io_handler(m->bound_handler);
        //#endif
        hash_destroy_table(m->connections,close_connection);
        free(m);
}


/* Function:  init_connections
 * Arguments: t: the nal state for this interface
 * Returns: a newly allocated manager structure, or
 *          zero if the fixed port could not be bound
 */
manager 
init_connections(int (*input)(void *, void *), void *a)
{
    manager m = (manager)malloc(sizeof(struct manager));
    bridge  b;

    if (m == NULL) 
            goto fail;
    
    m->connections = hash_create_table(compare_connection,connection_key);
    m->handler = input;
    m->handler_arg = b = a;
    m->port = 0;                         /* set on first connection */
       
    psc_warnx("nid %s tid %d b %p", libcfs_nid2str(b->b_ni->ni_nid), b->tid, b);

    pthread_mutex_init(&m->conn_lock, 0);

    if (tcpnal_server) {
            if (bind_socket(m, tcpnal_acceptor_port +
	        (tcpnal_portinc ? b->tid : 0)))
                    return (m);
    } else
            return (m);
 fail:
    return(0);
}
