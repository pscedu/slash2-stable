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
ECHORUN=	${ROOTDIR}/tools/echorun.sh
GENTYPES=	${ROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${ROOTDIR}/tools/libdep.pl
MDPROC=		${ROOTDIR}/tools/mdproc.pl
MINVER=		${ROOTDIR}/tools/minver.pl
PCPP=		${ROOTDIR}/tools/pcpp.pl

QMAKE=		${MAKE} >/dev/null 2>&1
MAKEFLAGS+=	--no-print-directory

LFLAGS+=	-t $$(if ${MINVER} $$(lex -V | sed 's/[a-z ]*//g') 2.5.5; then echo --nounput; fi)
YFLAGS+=	-d

CFLAGS+=	-Wall -W

ifdef DEBIAN
MPICC=		mpicc.mpich
else
MPICC=		mpicc
endif

DEBUG?=		1
ifeq (${DEBUG},1)
CFLAGS+=	-g
#CFLAGS+=	-ggdb3
else
CFLAGS+=	-Wunused -Wuninitialized -O2
#CFLAGS+=	-Wshadow
endif

DEFINES+=	-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
DEFINES+=	-D_REENTRANT -D_GNU_SOURCE -DYY_NO_UNPUT -DYY_NO_INPUT -DYYERROR_VERBOSE
DEFINES+=	-DHAVE_GETHOSTBYNAME
#DEFINES+=	-DDEBUG

KERNEL_BASE=	/usr/src/kernels/linux

FUSE_CFLAGS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_CFLAGS})
FUSE_DEFINES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_DEFINES})
FUSE_INCLUDES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_INCLUDES})
FUSE_LIBS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --libs fuse)
FUSE_VERSION=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --modversion fuse | sed 's/\([0-9]\)*\.\([0-9]*\).*/\1\2/')

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
THREAD_LIBS=	-pthread
LIBCURSES=	-lncurses

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

OSTYPE:=					$(shell uname)

# system-specific settings/overrides
ifneq ($(wildcard /opt/sgi),)
  # on altix
  NUMA_DEFINES=					-DCONFIG_NR_CPUS=2 -DHAVE_NUMA
  NUMA_LIBS=					-lcpuset -lbitmask -lnuma
  LIBL=						-lfl

  slash_nara_mount_slash_obj_lconf_c_PCPP_FLAGS=	-x
  slash_nara_slashd_obj_lconf_c_PCPP_FLAGS=		-x
  slash_nara_sliod_obj_lconf_c_PCPP_FLAGS=		-x
  slash_nara_tests_config_slash_obj_lconf_c_PCPP_FLAGS=	-x

  zest_zestFormat_obj_zestLexConfig_c_PCPP_FLAGS=	-x
  zest_zestion_obj_zestLexConfig_c_PCPP_FLAGS=		-x
endif

ifneq ($(wildcard /opt/xt-pe),)
  # on XT3
  QKCC=							qk-gcc
endif

ifeq (${OSTYPE},Linux)
  THREAD_LIBS+=						-lrt
endif

include ${ROOTDIR}/mk/pickle.mk

ifndef PICKLE_HAVE_POSIX_MEMALIGN
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/posix_memalign.c
endif

ifdef PICKLE_HAVE_PTHREAD_MUTEX_TIMEDLOCK
  DEFINES+=						-DHAVE_PTHREAD_MUTEX_TIMEDLOCK
endif

ifndef PICKLE_HAVE_CLOCK_GETTIME
  SRCS+=						${ROOTDIR}/psc_fsutil_libs/compat/clock_gettime.c
endif

ifndef PICKLE_HAVE_HOST_NAME_MAX
  DEFINES+=						-DHOST_NAME_MAX=MAXHOSTNAMELEN
endif
