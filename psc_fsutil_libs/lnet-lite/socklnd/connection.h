/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (c) 2002 Cray Inc.
 *
 *   This file is part of Portals, http://www.sf.net/projects/sandiaportals/
 */

#include <table.h>
#include <procbridge.h>
#include "psc_ds/listcache.h"

typedef struct manager {
        table            connections;
        list_cache_t    *conn_socks;      
        pthread_mutex_t  conn_lock; /* protect connections table */
        int              bound;
        io_handler       bound_handler;
        int            (*handler)(void *, void *);
        void            *handler_arg;
        int              port;
} *manager;


typedef struct connection {
        lnet_nid_t  peer_nid;
        lnet_pid_t  peer_pid; 
        int         fd;
        manager     m;
        io_handler  conn_iohandler;
} *connection;

connection force_tcp_connection(manager m, lnet_process_id_t *nidpid, 
                                procbridge pb);

connection force_tcp_connection_old(manager m, lnet_nid_t nid,
                                    procbridge pb);

manager init_connections(int (*f)(void *, void *), void *);
void remove_connection(void *arg);
void shutdown_connections(manager m);
int read_connection(connection c, unsigned char *dest, int len);
