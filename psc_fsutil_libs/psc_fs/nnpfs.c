/* $Id$ */

#include <sys/types.h>

#include <nnpfs/nnpfs_message.h>

#include <stdio.h>

int pscfs_nnpfs_kernelfd;

int
pscfs_nnpfs_opendev(const char *fn)
{
	pscfs_nnpfs_kernelfd = open(fd, O_RDWR);
}

pscfs_nnpfs_opendev(const char *fn)




int
pscfs_reply_open()
{
	struct nnpfs_message_open {
		struct nnpfs_message_header header;
		struct nnpfs_cred cred;
		nnpfs_handle handle;
		uint32_t tokens;
		uint32_t pad1;
	};

}









/*
 * Messages passed through the  nnpfs_dev.
 */
struct nnpfs_message_header {
};

/*
 * Used by putdata flag
 */
enum { NNPFS_READ     = 0x01,
       NNPFS_WRITE    = 0x02,
       NNPFS_NONBLOCK = 0x04,
       NNPFS_APPEND   = 0x08,
       NNPFS_FSYNC    = 0x10};

/*
 * Flags for inactivenode
 */
enum { NNPFS_NOREFS = 1, NNPFS_DELETE = 2 };

/*
 * Flags for installdata
 */

enum { NNPFS_ID_INVALID_DNLC = 0x01, NNPFS_ID_AFSDIR = 0x02,
       NNPFS_ID_HANDLE_VALID = 0x04 };

/*
 * Defined message types and their opcodes.
 */
#define NNPFS_MSG_VERSION		0
#define NNPFS_MSG_WAKEUP		1

#define NNPFS_MSG_GETROOT		2
#define NNPFS_MSG_INSTALLROOT	3

#define NNPFS_MSG_GETNODE		4
#define NNPFS_MSG_INSTALLNODE	5

#define NNPFS_MSG_GETATTR		6
#define NNPFS_MSG_INSTALLATTR	7

#define NNPFS_MSG_GETDATA		8
#define NNPFS_MSG_INSTALLDATA	9

#define NNPFS_MSG_INACTIVENODE	10
#define NNPFS_MSG_INVALIDNODE	11
		/* XXX Must handle dropped/revoked tokens better */

#define NNPFS_MSG_OPEN		12

#define NNPFS_MSG_PUTDATA		13
#define NNPFS_MSG_PUTATTR		14

/* Directory manipulating messages. */
#define NNPFS_MSG_CREATE		15
#define NNPFS_MSG_MKDIR		16
#define NNPFS_MSG_LINK		17
#define NNPFS_MSG_SYMLINK		18

#define NNPFS_MSG_REMOVE		19
#define NNPFS_MSG_RMDIR		20

#define NNPFS_MSG_RENAME		21

#define NNPFS_MSG_PIOCTL		22
#define NNPFS_MSG_WAKEUP_DATA	23

#define NNPFS_MSG_UPDATEFID	24

#define NNPFS_MSG_ADVLOCK		25

#define NNPFS_MSG_GC_NODES	26

#define NNPFS_MSG_COUNT		27

/* NNPFS_MESSAGE_VERSION */
struct nnpfs_message_version {
  struct nnpfs_message_header header;
  uint32_t ret;
};

/* NNPFS_MESSAGE_WAKEUP */
struct nnpfs_message_wakeup {
  struct nnpfs_message_header header;
  uint32_t sleepers_sequence_num;	/* Where to send wakeup */
  uint32_t error;			/* Return value */
};

/* NNPFS_MESSAGE_GETROOT */
struct nnpfs_message_getroot {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
};

/* NNPFS_MESSAGE_INSTALLROOT */
struct nnpfs_message_installroot {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETNODE */
struct nnpfs_message_getnode {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
};

/* NNPFS_MESSAGE_INSTALLNODE */
struct nnpfs_message_installnode {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETATTR */
struct nnpfs_message_getattr {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
};

/* NNPFS_MESSAGE_INSTALLATTR */
struct nnpfs_message_installattr {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETDATA */
struct nnpfs_message_getdata {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
  uint32_t tokens;
  uint32_t pad1;
  uint32_t offset;
  uint32_t pad2;
};

/* NNPFS_MESSAGE_INSTALLDATA */
struct nnpfs_message_installdata {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
  char cache_name[NNPFS_MAX_NAME];
  struct nnpfs_cache_handle cache_handle;
  uint32_t flag;
  uint32_t pad1;
  uint32_t offset;
  uint32_t pad2;
};

/* NNPFS_MSG_INACTIVENODE */
struct nnpfs_message_inactivenode {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  uint32_t flag;
  uint32_t pad1;
};

/* NNPFS_MSG_INVALIDNODE */
struct nnpfs_message_invalidnode {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
};

/* NNPFS_MSG_OPEN */
struct nnpfs_message_open {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
  uint32_t tokens;
  uint32_t pad1;
};

/* NNPFS_MSG_PUTDATA */
struct nnpfs_message_putdata {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_attr attr;		/* XXX ??? */
  struct nnpfs_cred cred;
  uint32_t flag;
  uint32_t pad1;
};

/* NNPFS_MSG_PUTATTR */
struct nnpfs_message_putattr {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_CREATE */
struct nnpfs_message_create {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_attr attr;
  uint32_t mode;
  uint32_t pad1;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_MKDIR */
struct nnpfs_message_mkdir {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_LINK */
struct nnpfs_message_link {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  nnpfs_handle from_handle;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_SYMLINK */
struct nnpfs_message_symlink {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  char contents[NNPFS_MAX_SYMLINK_CONTENT];
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_REMOVE */
struct nnpfs_message_remove {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_RMDIR */
struct nnpfs_message_rmdir {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_RENAME */
struct nnpfs_message_rename {
  struct nnpfs_message_header header;
  nnpfs_handle old_parent_handle;
  char old_name[NNPFS_MAX_NAME];
  nnpfs_handle new_parent_handle;
  char new_name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

#define NNPFS_MSG_MAX_DATASIZE	2048

/* NNPFS_MSG_PIOCTL */
struct nnpfs_message_pioctl {
  struct nnpfs_message_header header;
  uint32_t opcode ;
  uint32_t pad1;
  nnpfs_cred cred;
  uint32_t insize;
  uint32_t outsize;
  char msg[NNPFS_MSG_MAX_DATASIZE];
  nnpfs_handle handle;
};


/* NNPFS_MESSAGE_WAKEUP_DATA */
struct nnpfs_message_wakeup_data {
  struct nnpfs_message_header header;
  uint32_t sleepers_sequence_num;	/* Where to send wakeup */
  uint32_t error;			/* Return value */
  uint32_t len;
  uint32_t pad1;
  char msg[NNPFS_MSG_MAX_DATASIZE];
};

/* NNPFS_MESSAGE_UPDATEFID */
struct nnpfs_message_updatefid {
  struct nnpfs_message_header header;
  nnpfs_handle old_handle;
  nnpfs_handle new_handle;
};

/* NNPFS_MESSAGE_ADVLOCK */
struct nnpfs_message_advlock {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_cred cred;
  nnpfs_locktype_t locktype;
#define NNPFS_WR_LOCK 1 /* Write lock */
#define NNPFS_RD_LOCK 2 /* Read lock */
#define NNPFS_UN_LOCK 3 /* Unlock */
#define NNPFS_BR_LOCK 4 /* Break lock (inform that we don't want the lock) */
  nnpfs_lockid_t lockid;
};

/* NNPFS_MESSAGE_GC_NODES */
struct nnpfs_message_gc_nodes {
  struct nnpfs_message_header header;
#define NNPFS_GC_NODES_MAX_HANDLE 50
  uint32_t len;
  uint32_t pad1;
  nnpfs_handle handle[NNPFS_GC_NODES_MAX_HANDLE];
};

#if 0 
struct nnpfs_name {
    u_int16_t name;
    char name[1];
};
#endif

struct nnpfs_message_bulkgetnode {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  uint32_t flags;
#define NNPFS_BGN_LAZY		0x1
  uint32_t numnodes;
  struct nnpfs_handle handles[1];
};

#endif /* _xmsg_h */
