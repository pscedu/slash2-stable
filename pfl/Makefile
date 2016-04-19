# $Id$

ROOTDIR=..
include ${ROOTDIR}/Makefile.path

SUBDIRS+=	tests
SUBDIRS+=	utils
LIBRARY=	libpfl.a
NOINSTALL=	1

SRCS+=		${PFL_BASE}/acsvc.c
SRCS+=		${PFL_BASE}/alloc.c
SRCS+=		${PFL_BASE}/base64.c
SRCS+=		${PFL_BASE}/bsearch.c
SRCS+=		${PFL_BASE}/completion.c
SRCS+=		${PFL_BASE}/dbgutil.c
SRCS+=		${PFL_BASE}/dynarray.c
SRCS+=		${PFL_BASE}/err_pfl.c
SRCS+=		${PFL_BASE}/fault.c
SRCS+=		${PFL_BASE}/fmt.c
SRCS+=		${PFL_BASE}/fts.c
SRCS+=		${PFL_BASE}/hashtbl.c
SRCS+=		${PFL_BASE}/heap.c
SRCS+=		${PFL_BASE}/init.c
SRCS+=		${PFL_BASE}/list.c
SRCS+=		${PFL_BASE}/listcache.c
SRCS+=		${PFL_BASE}/lockedlist.c
SRCS+=		${PFL_BASE}/log.c
SRCS+=		${PFL_BASE}/memnode.c
SRCS+=		${PFL_BASE}/meter.c
SRCS+=		${PFL_BASE}/mkdirs.c
SRCS+=		${PFL_BASE}/mlist.c
SRCS+=		${PFL_BASE}/multiwait.c
SRCS+=		${PFL_BASE}/netutil.c
SRCS+=		${PFL_BASE}/odtable.c
SRCS+=		${PFL_BASE}/opstats.c
SRCS+=		${PFL_BASE}/opt-misc.c
SRCS+=		${PFL_BASE}/pool.c
SRCS+=		${PFL_BASE}/printhex.c
SRCS+=		${PFL_BASE}/prsig.c
SRCS+=		${PFL_BASE}/pthrutil.c
SRCS+=		${PFL_BASE}/random.c
SRCS+=		${PFL_BASE}/rlimit.c
SRCS+=		${PFL_BASE}/setprocesstitle.c
SRCS+=		${PFL_BASE}/str.c
SRCS+=		${PFL_BASE}/stree.c
SRCS+=		${PFL_BASE}/subsys.c
SRCS+=		${PFL_BASE}/sys.c
SRCS+=		${PFL_BASE}/thread.c
SRCS+=		${PFL_BASE}/timerthr.c
SRCS+=		${PFL_BASE}/vbitmap.c
SRCS+=		${PFL_BASE}/waitq.c
SRCS+=		${PFL_BASE}/walk.c
SRCS+=		${PFL_BASE}/workthr.c

SRCS+=		${ACL_SRCS}
SRCS+=		${CLOCK_SRCS}
SRCS+=		${SETRESUID_SRC}
SRCS+=		${STRVIS_SRCS}

MODULES+=	lnet-hdrs pthread clock rt acl

include ${PFLMK}
