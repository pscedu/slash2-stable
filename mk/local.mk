# $Id$

MKDEP=		env CC="${CC}" ${ROOTDIR}/tools/unwrapcc ${ROOTDIR}/tools/mkdep

# Disappointingly, recent versions of gcc hide
# standard headers in places other than /usr/include.
LIBC_INCLUDES+=	$$(if ${CC} -v 2>&1 | grep -q gcc; then ${CC} -print-search-dirs | \
		    grep install | awk '{print "-I" $$2 "include"}' | sed 's/:/ -I/'; fi)
LINT=		splint +posixlib
NOTEMPTY=	${ROOTDIR}/tools/notempty
SCONS=		scons
PKG_CONFIG=	pkg-config
GENTYPES=	${ROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${ROOTDIR}/tools/libdep.pl
ECHORUN=	${ROOTDIR}/tools/echorun.sh
MAKEFLAGS+=	--no-print-directory

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

KERNEL_BASE=	/usr/src/kernels/linux

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

FUSE_CFLAGS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_CFLAGS})
FUSE_DEFINES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_DEFINES})
FUSE_INCLUDES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_INCLUDES})
FUSE_LIBS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --libs fuse)

ZFS_LIBS=	-L${ZFS_BASE}/zfs-fuse					\
		-L${ZFS_BASE}/lib/libavl				\
		-L${ZFS_BASE}/lib/libnvpair				\
		-L${ZFS_BASE}/lib/libsolkerncompat			\
		-L${ZFS_BASE}/lib/libumem				\
		-L${ZFS_BASE}/lib/libzfscommon				\
		-L${ZFS_BASE}/lib/libzpool				\
		-lzfs-fuse -lzpool-kernel -lzfscommon-kernel		\
		-lnvpair-kernel -lavl -lumem -lsolkerncompat -ldl

LIBL=		-ll
LIBZ=		-lz
THREAD_LIBS=	-pthread -lrt

# global file-specific settings
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
