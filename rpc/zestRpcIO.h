/* $Id: zestRpcIO.h 2151 2007-11-05 21:58:33Z pauln $ */

#ifndef ZESTRPCIO_H
#define ZESTRPCIO_H

#include "zestTypes.h"
#include "iostats.h"
#include "ciod.h"

#define NUM_RPCIO_THREADS 24

#define RPCIO_NBUFS      3072
#define RPCIO_BUFSIZE    384
#define RPCIO_MAXREQSIZE RPCIO_BUFSIZE
#define RPCIO_MAXREPSIZE 256

#define RPCIO_REQUEST_PORTAL 16
#define RPCIO_REPLY_PORTAL   17
#define RPCIO_BULK_PORTAL    21

#define ZIO_CONNECT_MAGIC 0x1100ffeeddccbbaaULL
#define ZIO_VERSION       0x1

#define ZSB_CB_POINTER_SLOT 1

#define RPCIO_SVCNAME "zrpc_io"

struct zestrpc_request;
struct zestrpc_service;

/* Zest RPC I/O operations (opcodes). */
#define ZIO_WRITE	10
#define ZIO_CONNECT	11

struct zio_connect_body {
	u64 zicb_magic;
	u32 zicb_version;
};

struct zio_write_body {
	struct ciod_wire zwb_ciodw;	
};

struct zio_reply_body {
	zest_crc_t       crc_meta_magic;
	u32              nbytes;
};

extern struct zestrpc_service *zestRpcIoSvc;
extern struct iostats rpcIOStats;

int zrpc_io_handler(struct zestrpc_request *req);

#endif /* ZESTRPCIO_H */
