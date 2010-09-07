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
LIBGCRYPT_CONFIG=libgcrypt-config
MPICC=		mpicc
ECHORUN=	${ROOTDIR}/tools/echorun.sh
_PERLENV=	PERL5LIB=${PERL5LIB}:${CROOTDIR}/tools/lib
GENTYPES=	${_PERLENV} ${CROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${ROOTDIR}/tools/libdep.pl
MDPROC=		${ROOTDIR}/tools/mdproc.pl
MINVER=		${ROOTDIR}/tools/minver.pl
PCPP=		${_PERLENV} ${CROOTDIR}/tools/pcpp.pl
PICKLEGEN=	${ROOTDIR}/tools/pickle-gen.sh

MAKEFLAGS+=	--no-print-directory

LFLAGS+=	-t $$(if ${MINVER} $$(lex -V | sed 's![a-z /]*!!g') 2.5.5; then echo --nounput; fi)
YFLAGS+=	-d

CFLAGS+=	-Wall -W

DEBUG?=		1
ifeq (${DEBUG},0)
  CFLAGS+=	-Wunused -Wuninitialized -O2
# CFLAGS+=	-Wshadow
else
  CFLAGS+=	-g
  DEFINES+=	-DDEBUG=${DEBUG}
endif

DEFINES+=	-D_REENTRANT -DYY_NO_UNPUT -DYY_NO_INPUT -DYYERROR_VERBOSE

KERNEL_BASE=	/usr/src/kernels/linux

FUSE_CFLAGS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_CFLAGS})
FUSE_DEFINES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_DEFINES})
FUSE_INCLUDES=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --cflags fuse | ${EXTRACT_INCLUDES})
FUSE_LIBS=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --libs fuse)
FUSE_VERSION=	$$(PKG_CONFIG_PATH=${FUSE_BASE} ${PKG_CONFIG} --modversion fuse | sed 's/\([0-9]\)*\.\([0-9]*\).*/\1\2/')

GCRYPT_CFLAGS=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_CFLAGS})
GCRYPT_DEFINES=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_DEFINES})
GCRYPT_INCLUDES=$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_INCLUDES})
GCRYPT_LIBS=	$$(${LIBGCRYPT_CONFIG} --libs)

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
LIBAIO=		-laio

OSTYPE:=	$(shell uname)

# global file-specific settings
$(call ADD_FILE_CFLAGS,${PFL_BASE}/psc_util/crc.c,			-O2 -g0)
$(call ADD_FILE_CFLAGS,${PFL_BASE}/psc_util/parity.c,			-O2 -g0)

$(call ADD_FILE_CFLAGS,${LNET_BASE}/ulnds/socklnd/conn.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/ulnds/socklnd/handlers.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/ulnds/socklnd/poll.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/ulnds/socklnd/usocklnd.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c,	-DPSC_SUBSYS=PSS_LNET -Wno-shadow)

$(call ADD_FILE_CFLAGS,${LNET_BASE}/libcfs/debug.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/libcfs/nidstrings.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/libcfs/user-lock.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/libcfs/user-prim.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/libcfs/user-tcpip.c,		-DPSC_SUBSYS=PSS_LNET -Wno-shadow)

$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/acceptor.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/api-errno.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/api-ni.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/config.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lib-eq.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lib-md.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lib-me.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lib-move.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lib-msg.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/lo.c,				-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/peer.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/router.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)
$(call ADD_FILE_CFLAGS,${LNET_BASE}/lnet/router_proc.c,			-DPSC_SUBSYS=PSS_LNET -Wno-shadow)

# system-specific settings/overrides
ifneq ($(wildcard /opt/sgi),)
  # on altix
  NUMA_DEFINES=					-DHAVE_NUMA
  NUMA_LIBS=					-lcpuset -lbitmask -lnuma
  LIBL=						-lfl

  slash_nara_mount_slash_obj_lconf_c_PCPP_FLAGS=	-H yytext
  slash_nara_slashd_obj_lconf_c_PCPP_FLAGS=		-H yytext
  slash_nara_sliod_obj_lconf_c_PCPP_FLAGS=		-H yytext
  slash_nara_tests_config_obj_lconf_c_PCPP_FLAGS=	-H yytext

  slash_nara_mount_slash_obj_yconf_c_PCPP_FLAGS=	-H yytext
  slash_nara_slashd_obj_yconf_c_PCPP_FLAGS=		-H yytext
  slash_nara_sliod_obj_yconf_c_PCPP_FLAGS=		-H yytext
  slash_nara_tests_config_obj_yconf_c_PCPP_FLAGS=	-H yytext

  zest_zestFormat_obj_zestLexConfig_c_PCPP_FLAGS=	-H yytext
  zest_zestiond_obj_zestLexConfig_c_PCPP_FLAGS=		-H yytext
  zest_tests_config_obj_zestLexConfig_c_PCPP_FLAGS=	-H yytext
endif

ifneq ($(wildcard /opt/xt-os),)
  # on XT3
  QKCC=						qk-gcc
  LIBL=						-lfl
  DEFINES+=					-DHAVE_CNOS
endif

ifeq (${OSTYPE},Linux)
  LIBRT=					-lrt
  DEFINES+=					-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
  DEFINES+=					-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq (${OSTYPE},Darwin)
  DEFINES+=					-D_DARWIN_C_SOURCE -D_DARWIN_FEATURE_64_BIT_INODE
endif

ifeq (${OSTYPE},OpenBSD)
  DEFINES+=					-D_BSD_SOURCE
endif
