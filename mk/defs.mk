# $Id$

LINT?=		splint +posixlib
MD5?=		md5sum
SHA1?=		sha1sum
SHA256?=	sha256sum
SCONS=		scons DEFINES="$(filter -DHAVE_%,${DEFINES}) $(filter -DPFL_%,${DEFINES})"
PKG_CONFIG_PATH?=/usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig
PKG_CONFIG?=	PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config
LIBGCRYPT_CONFIG?=libgcrypt-config
MPICC?=		mpicc
ROFF?=		nroff
INSTALL?=	install

NOTEMPTY=	${ROOTDIR}/tools/notempty
ECHORUN=	${ROOTDIR}/tools/echorun.sh
_PERLENV=	env PERL5LIB=${PERL5LIB}:${CROOTDIR}/tools/lib
GENTYPES=	${_PERLENV} ${CROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${ROOTDIR}/tools/libdep.pl
MDPROC=		${_PERLENV} ${CROOTDIR}/tools/mdproc.pl -D PFL_BASE=${PFL_BASE}
MINVER=		${ROOTDIR}/tools/minver.pl
PCPP=		${_PERLENV} ${CROOTDIR}/tools/pcpp.pl
PICKLEGEN=	${ROOTDIR}/tools/pickle-gen.sh
MKDIRS=		${ROOTDIR}/tools/mkdirs
CLEAR_EOL=	(tput el || tput ce)
INST=		inst() {						\
			(while [ $$\# -gt 1 ]; do			\
				shift;					\
			 done ; mkdir -p $$1);				\
			 ${ECHORUN} ${INSTALL} -C -b "$$@";		\
		} ; inst

LEX:=		$(shell if ${NOTEMPTY} "${LEX}"; then			\
			echo "${LEX}";					\
		elif type flex; then					\
			echo flex;					\
		else							\
			echo lex;					\
		fi)

INST_BASE?=	/usr/psc
INST_BINDIR?=	${INST_BASE}/bin
INST_SBINDIR?=	${INST_BASE}/sbin
INST_LIBDIR?=	${INST_BASE}/lib
INST_LIBDIR?=	${INST_BASE}/include
INST_MANDIR?=	${INST_BASE}/man
INST_ETCDIR?=	${INST_BASE}/etc
INST_PLMODDIR?=	${INST_BASE}/lib/perl5

MAKEFLAGS+=	--no-print-directory

# XXX hack, figure out why
ifdef CFLAGS
  MAKE:=	env CFLAGS="${CFLAGS}" ${MAKE}
endif

LFLAGS+=	-t $$(if ${MINVER} $$(${LEX} -V | \
		  sed 's!^[^0-9]*!!; s/ .*//') 2.5.5; then \
		  echo --nounput; fi)

YFLAGS+=	-d

CFLAGS+=	-Wall -W -pipe
# -Wredundant-decls
# CFLAGS+=	-Wshadow

DEBUG?=		1
DEVELPATHS?=	1
ifeq (${DEBUG},0)
  CFLAGS+=	-Wunused -Wuninitialized -O2 -fno-strict-aliasing
else
  CFLAGS+=	-g
  LDFLAGS+=	-fstack-protector-all
endif

DEFINES+=	-D_REENTRANT -DYY_NO_UNPUT -DYY_NO_INPUT -DYYERROR_VERBOSE
DEFINES+=	-DPFL_DEBUG=${DEBUG} -DDEVELPATHS=${DEVELPATHS}

INCLUDES+=	-I${ROOTDIR} -I.
EXCLUDES+=	-I${PFL_BASE}

ifneq ($(wildcard /local/tmp),)
  OBJBASE?=		/local/tmp
else
  ifneq ($(wildcard /log/tmp),)
    OBJBASE?=		/log/tmp
  else
    OBJBASE?=		/tmp
  endif
endif

KERNEL_BASE=	/usr/src/kernels/linux

COPYRIGHT_PATS+='*.[chyl]'							\
		'*.[0-9]'							\
		'*.mk'								\
		'*.p[ml]'							\
		'*.sh'								\
		Makefile

FUSE_CFLAGS=	$$(${PKG_CONFIG} --cflags fuse | ${EXTRACT_CFLAGS})
FUSE_DEFINES=	$$(${PKG_CONFIG} --cflags fuse | ${EXTRACT_DEFINES})
FUSE_INCLUDES=	$$(${PKG_CONFIG} --cflags fuse | ${EXTRACT_INCLUDES})
FUSE_LIBS=	$$(${PKG_CONFIG} --libs fuse)
FUSE_VERSION=	$$(${PKG_CONFIG} --modversion fuse | sed 's/\([0-9]\)*\.\([0-9]*\).*/\1\2/')

GCRYPT_CFLAGS=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_CFLAGS})
GCRYPT_DEFINES=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_DEFINES})
GCRYPT_INCLUDES=$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_INCLUDES})
GCRYPT_LIBS=	$$(${LIBGCRYPT_CONFIG} --libs)

ZFS_LIBS=	-L${ZFS_BASE}/src/zfs-fuse				\
		-L${ZFS_BASE}/src/lib/libavl				\
		-L${ZFS_BASE}/src/lib/libnvpair				\
		-L${ZFS_BASE}/src/lib/libsolkerncompat			\
		-L${ZFS_BASE}/src/lib/libumem				\
		-L${ZFS_BASE}/src/lib/libzfscommon			\
		-L${ZFS_BASE}/src/lib/libzpool				\
		-lzfs-fuse -lslzpool-kernel -lzfscommon-kernel		\
		-lnvpair-kernel -lavl -lumem -lslsolkerncompat -ldl -lcrypto

LIBL=		-ll
LIBZ=		-lz
THREAD_LIBS=	-pthread
LIBCURSES=	-lncurses
LIBAIO=		-laio

OSTYPE:=	$(shell uname)

# global file-specific settings
$(call ADD_FILE_CFLAGS,${PFL_BASE}/crc.c,				-O2 -g0)
$(call ADD_FILE_CFLAGS,${PFL_BASE}/parity.c,				-O2 -g0)

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

# system-specific settings
ifneq ($(wildcard /opt/sgi),)
  LIBL=		-lfl
  PCPP_FLAGS+=	-H yytext
endif

ifeq (${OSTYPE},Linux)
  LIBRT=	-lrt
  DEFINES+=	-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
  DEFINES+=	-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq (${OSTYPE},Darwin)
  DEFINES+=	-D_DARWIN_C_SOURCE -D_DARWIN_FEATURE_64_BIT_INODE
  DEFINES+=	-DHAVE_NO_POLL_DEV
endif

ifeq (${OSTYPE},OpenBSD)
  DEFINES+=	-D_BSD_SOURCE
endif

-include ${ROOTDIR}/mk/local.$(shell hostname).mk
-include ${ROOTDIR}/mk/local.mk
