# $Id$

ROOTDIR=../..
include ${ROOTDIR}/Makefile.path

PROG=		mount_glaze
MAN+=		mount_glaze.8
SRCS+=		main_glaze.c
SRCS+=		${PFL_BASE}/psc_ds/hashtbl.c
SRCS+=		${PFL_BASE}/psc_ds/listcache.c
SRCS+=		${PFL_BASE}/psc_ds/stree.c
SRCS+=		${PFL_BASE}/psc_ds/vbitmap.c
SRCS+=		${PFL_BASE}/psc_util/iostats.c
SRCS+=		${PFL_BASE}/psc_util/pool.c
SRCS+=		${PFL_BASE}/psc_util/pthrutil.c
SRCS+=		${PFL_BASE}/psc_util/random.c
SRCS+=		${PFL_BASE}/psc_util/rlimit.c
SRCS+=		${PFL_BASE}/psc_util/thread.c
SRCS+=		${PFL_BASE}/psc_util/timerthr.c
SRCS+=		${PFL_BASE}/psc_util/waitq.c

MODULES+=	pthread pscfs-hdrs pfl fuse ctl

include ${GLAZEMK}
