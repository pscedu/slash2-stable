/* $Id: zestRpcMDS.h 2114 2007-11-03 19:39:08Z pauln $ */

#ifndef ZESTRPCMDS_H
#define ZESTRPCMDS_H 1

#include "zestFid.h"

#define NUM_RPCMDS_THREADS 8

#define RPCMDS_NBUFS      128
#define RPCMDS_BUFSIZE    256
#define RPCMDS_MAXREQSIZE RPCMDS_BUFSIZE
#define RPCMDS_MAXREPSIZE 256

#define RPCMDS_REQUEST_PORTAL 18
#define RPCMDS_REPLY_PORTAL   19
#define RPCMDS_BULK_PORTAL    20

#define ZMDS_CONNECT_MAGIC 0xaabbccddeeff0011ULL
#define ZMDS_VERSION       0x1

#define RPCMDS_SVCNAME "zrpc_mds"

struct zestrpc_request;

struct zestrpc_service *zestRpcMdsSvc;

enum zmds_opcodes {
	ZMDS_CREAT = 0,
	ZMDS_OPEN,
	ZMDS_CLOSE,
	ZMDS_FSTAT,
	ZMDS_TRUNC,
	ZMDS_MKDIR,
	ZMDS_CONNECT
};


struct zmds_connect_body {
	u64 fsmagic;
	u32 fsversion;
};
/*
 * Rpc message for a create request
 */
struct zmds_create_body {
	u32 uid;
	u32 gid;
	u32 flags;
	u32 mode;
	u32 flen;
	u32 fd;
};

struct zmds_close_body {
	int clientfd;
	u64 fdmagic;
};

struct zmds_fstat_body {
	int zfb_clientfd;
	u64 zfb_clientfdmagic;
};

struct zmds_fstat_repbody {
	u64 zfrb_magic;
	u64 zfrb_size;
	u64 zfrb_ino;
	u32 zfrb_ctime;
	u32 zfrb_mtime;
	u32 zfrb_atime;
	u32 zfrb_blksize;
	u32 zfrb_blocks;
	u32 zfrb_rdev;
	u32 zfrb_uid;
	u32 zfrb_gid;
	u32 zfrb_nlink;
};

extern
int zrpc_mds_handler(struct zestrpc_request *req);

#endif /* ZESTRPCMDS_H */
