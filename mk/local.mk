# $Id$

MKDEP_PROG=	env CC="${CC}" ${ROOTDIR}/tools/unwrapcc ${ROOTDIR}/tools/mkdep -f ${DEPEND_FILE}

# Disappointingly, recent versions of gcc hide
# standard headers in places other than /usr/include.
MKDEP=		${MKDEP_PROG} -D ${OBJDIR} $$(if ${CC} -v 2>&1 | grep -q gcc; then \
		    ${CC} -print-search-dirs | grep install | \
		    awk '{print "-I" $$2 "include"}' | sed 's/:/ -I/'; fi)
LINT=		splint +posixlib
NOTEMPTY=	${ROOTDIR}/tools/notempty
SCONS=		scons
PKG_CONFIG_PROG=pkg-config
PKG_CONFIG=	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ${PKG_CONFIG_PROG}
GENTYPES=	${ROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${ROOTDIR}/tools/libdep.pl
ECHORUN=	${ROOTDIR}/tools/echorun.sh
MAKEFLAGS+=	--no-print-directory

# XXX this needs bumped
LKERNEL_BASE=	${ROOTDIR}/kernel/2.6.9-42.0.8.EL_lustre.1.4.9.1

LFLAGS+=	-t
YFLAGS+=	-d

CFLAGS+=	-Wall -W
CFLAGS+=	-g
#CFLAGS+=	-ggdb3
#CFLAGS+=	-Wunused -Wuninitialized -O
#CFLAGS+=	-Wshadow
DEFINES+=	-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
DEFINES+=	-D_REENTRANT -D_GNU_SOURCE -DYY_NO_UNPUT
DEFINES+=	-DHAVE_GETHOSTBYNAME

LIBL?=		-ll

KERNEL_BASE=	/usr/src/kernels/linux

ifneq ($(filter fuse,${MODULES}),)
CFLAGS+=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG_PROG} --cflags fuse | ${EXTRACT_CFLAGS})
DEFINES+=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG_PROG} --cflags fuse | ${EXTRACT_DEFINES})
INCLUDES+=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG_PROG} --cflags fuse | ${EXTRACT_INCLUDES})
LDFLAGS+=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG_PROG} --libs fuse)
endif

ifneq ($(filter zfs,${MODULES}),)
INCLUDES+=	-I${ZFS_BASE}

LDFLAGS+=	-L${ZFS_BASE}/zfs-fuse				\
		-L${ZFS_BASE}/lib/libavl			\
		-L${ZFS_BASE}/lib/libnvpair			\
		-L${ZFS_BASE}/lib/libsolkerncompat		\
		-L${ZFS_BASE}/lib/libumem			\
		-L${ZFS_BASE}/lib/libzfscommon			\
		-L${ZFS_BASE}/lib/libzpool			\
		-lzfs-fuse -lzpool-kernel -lzfscommon-kernel	\
		-lnvpair-kernel -lavl -lumem -lsolkerncompat
endif

ifneq ($(filter lnet,${MODULES}),)
SRCS+=		${LNET_CFS_SRCS}
SRCS+=		${LNET_LIB_SRCS}

INCLUDES+=	-I${LNET_BASE}/include
endif

ifneq ($(filter lnet-hdrs,${MODULES}),)
INCLUDES+=	-I${LNET_BASE}/include
endif

ifneq ($(filter pthread,${MODULES}),)
LDFLAGS+=	-pthread -lrt
DEFINES+=	-DHAVE_LIBPTHREAD
endif

ifneq ($(wildcard /opt/sgi),)
  # on altix
  DEFINES+=	-DCONFIG_NR_CPUS=2 -DHAVE_NUMA
  NUMA_LIBS=	-lcpuset -lbitmask -lnuma
endif

ifneq ($(wildcard /opt/xt-pe),)
  # on xt3
  SRCS+=	${PFL_BASE}/compat/posix_memalign.c
  DEFINES+=	-DHOST_NAME_MAX=MAXHOSTNAMELEN
endif

psc_fsutil_libs_psc_util_crc_c_CFLAGS=		-O2 -g0
psc_fsutil_libs_psc_util_parity_c_CFLAGS=	-O2 -g0

lnet_lite_ulnds_socklnd_conn_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_ulnds_socklnd_handlers_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_ulnds_socklnd_poll_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_ulnds_socklnd_usocklnd_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_ulnds_socklnd_usocklnd_cb_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET -Wno-shadow

lnet_lite_libcfs_debug_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_libcfs_nidstrings_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_libcfs_user_lock_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_libcfs_user_prim_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_libcfs_user_tcpip_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow

lnet_lite_lnet_acceptor_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_api_errno_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_api_ni_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_config_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lib_eq_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lib_md_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lib_me_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lib_move_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lib_msg_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_lo_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_peer_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_router_c_CFLAGS=			-DPSC_SUBSYS=PSS_LNET -Wno-shadow
lnet_lite_lnet_router_proc_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET -Wno-shadow
