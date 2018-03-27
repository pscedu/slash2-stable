# $Id$

# Globally applicable definitions for the PFL build stack.
# Note: pickle variables may not be accessed at parse time in this file.

LINT?=		splint +posixlib
MD5?=		md5sum
SHA1?=		sha1sum
SHA256?=	sha256sum
SCONS=		scons DEFINES="$(filter -DHAVE_%,${DEFINES}) $(filter -DPFL_%,${DEFINES})"
PKG_CONFIG_PATH?=/usr/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:/opt/local/lib/pkgconfig
PKG_CONFIG?=	PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config
LIBGCRYPT_CONFIG?=libgcrypt-config
MPICC?=		mpicc
ROFF?=		nroff
INSTALL?=	install

ECHORUN=	${ROOTDIR}/tools/echorun.sh
_PERLENV=	env PERL5LIB=${PERL5LIB}:${ROOTDIR}/tools/lib perl
NOTEMPTY=	${_PERLENV} ${ROOTDIR}/tools/notempty
GENTYPES=	${_PERLENV} ${ROOTDIR}/tools/gentypes.pl
HDRCLEAN=	${_PERLENV} ${ROOTDIR}/tools/hdrclean.pl
LIBDEP=		${_PERLENV} ${ROOTDIR}/tools/libdep.pl
MDPROC=		${_PERLENV} ${ROOTDIR}/tools/mdproc.pl -D PFL_BASE=${PFL_BASE}
MINVER=		${_PERLENV} ${ROOTDIR}/tools/minver.pl
PCPP=		${_PERLENV} ${ROOTDIR}/tools/pcpp.pl
PICKLEGEN=	${ROOTDIR}/tools/pickle-gen.sh
MKDIRS=		${ROOTDIR}/tools/mkdirs
SYNCMAKE=	${ROOTDIR}/tools/syncmake
XSLTPROC=	xsltproc
XDC2TEX_XSL=	${ROOTDIR}/tools/lib/xdc2tex.xsl
CLEAR_EOL=	(tput el || tput ce)
INST=		inst() {						\
			(while [ $$\# -gt 1 ]; do			\
				shift;					\
			 done ;						\
			 _dir="$$1";					\
			 ${ECHORUN} mkdir -p $${_dir%/*});		\
			 ${ECHORUN} ${INSTALL} -C -b "$$@";		\
		} ; inst

LEX:=		$(shell if ${NOTEMPTY} "${LEX}"; then			\
			echo "${LEX}";					\
		elif type flex; then					\
			echo flex;					\
		else							\
			echo lex;					\
		fi)

INST_BASE?=	/local
INST_BINDIR?=	${INST_BASE}/bin
INST_SBINDIR?=	${INST_BASE}/sbin
INST_LIBDIR?=	${INST_BASE}/lib
INST_INCDIR?=	${INST_BASE}/include
INST_MANDIR?=	${INST_BASE}/man
INST_ETCDIR?=	${INST_BASE}/etc
INST_PLMODDIR?=	${INST_BASE}/lib/perl5

MAKEFLAGS+=	--no-print-directory

# XXX hack, figure out why
ifdef CFLAGS
  MAKE:=	env CFLAGS="${CFLAGS}" ${MAKE}
endif

LEXVER=		$(shell ${LEX} -V | sed 's/^[^0-9]*//; s/ .*//')

ifeq ($(shell ${MINVER} ${LEXVER} 2.5.5 && echo 1),1)
  LFLAGS+=	--nounput
endif

LFLAGS+=	-t

YFLAGS+=	-d

CFLAGS+=	-Wall -Wextra -pipe
# -Wredundant-decls
CFLAGS+=	-Wshadow -fno-omit-frame-pointer
LDFLAGS+=	-fno-omit-frame-pointer
#CFLAGS+=	-Wno-address

# Bits to enable Google profiler.
ifeq (${GOPROF},1)
CFLAGS+=	-fno-builtin-malloc -fno-builtin-calloc $( \
		) -fno-builtin-realloc -fno-builtin-free
LDFLAGS+=	-ltcmalloc -lprofiler
endif

# Bits to enable efence.
ifeq (${EFENCE},1)
LDFLAGS+=	-lefence
endif

DEBUG?=		1
DEVELPATHS?=	0
ifeq (${DEBUG},0)
  CFLAGS+=	-Wunused -Wuninitialized
  CFLAGS+=	-fno-strict-aliasing
  CFLAGS+=	${COPT}

  GCC_VERSION=$(shell ${CC} -v 2>&1 | grep -o 'gcc version [0-9.]*' | awk '{print $$3}')
  ifeq ($(shell ${MINVER} "${GCC_VERSION}" 4.2.3 && echo 1),1)
	CFLAGS+=-flto -fno-use-linker-plugin
	LDFLAGS+=-flto -fno-use-linker-plugin
  endif
else
  CFLAGS+=	-g
  CFLAGS+=	-fstack-protector-all
  LDFLAGS+=	-fstack-protector-all

  CFLAGS+=	${FSANITIZE_CFLAGS}
  LDFLAGS+=	${FSANITIZE_LDFLAGS}
endif

COPT=		-g0 -O2

DEFINES+=	-D_REENTRANT -DYY_NO_UNPUT -DYY_NO_INPUT -DYYERROR_VERBOSE
DEFINES+=	-DPFL_DEBUG=${DEBUG} -DDEVELPATHS=${DEVELPATHS}

INCLUDES+=	-I${ROOTDIR} -I.
EXCLUDES+=	-I${PFL_BASE}

ifneq ($(wildcard /local/tmp),)
  OBJBASE?=	/local/tmp
else
  ifneq ($(wildcard /log/tmp),)
    OBJBASE?=	/log/tmp
  else
    OBJBASE?=	/tmp
  endif
endif

REPO_VERSION=	$$(git log --pretty=format:%h | grep -c .)

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
FUSE_KVERSION=	$$(${PKG_CONFIG} --version fuse)
FUSE_VERSION=	$$(${PKG_CONFIG} --modversion fuse | sed 's/\([0-9]\)*\.\([0-9]*\).*/\1\2/')

ifeq ($(shell ${MINVER} ${FUSE_KVERSION} 0.27.0 && echo 1),1)
  FUSE_DEFINES+=-DHAVE_FUSE_NOTIFY_INVAL
endif

SQLITE3_CFLAGS=	$$(${PKG_CONFIG} --cflags sqlite3 | ${EXTRACT_CFLAGS})
SQLITE3_DEFINES=$$(${PKG_CONFIG} --cflags sqlite3 | ${EXTRACT_DEFINES})
SQLITE3_INCLUDES=$$(${PKG_CONFIG} --cflags sqlite3 | ${EXTRACT_INCLUDES})
SQLITE3_LIBS=	$$(${PKG_CONFIG} --libs sqlite3)

GCRYPT_CFLAGS=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_CFLAGS})
GCRYPT_DEFINES=	$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_DEFINES})
GCRYPT_INCLUDES=$$(${LIBGCRYPT_CONFIG} --cflags | ${EXTRACT_INCLUDES})
GCRYPT_LIBS=	$$(${LIBGCRYPT_CONFIG} --libs) -lgpg-error

NUMA_LIBS=	-lcpuset -lnuma

ZFS_LIBS=	-L${ZFS_BASE}/src/zfs-fuse				\
		-L${ZFS_BASE}/src/lib/libavl				\
		-L${ZFS_BASE}/src/lib/libnvpair				\
		-L${ZFS_BASE}/src/lib/libsolkerncompat			\
		-L${ZFS_BASE}/src/lib/libumem				\
		-L${ZFS_BASE}/src/lib/libzfscommon			\
		-L${ZFS_BASE}/src/lib/libzpool				\
		-lzfs-fuse -lslzpool-kernel -lzfscommon-kernel		\
		-lnvpair-kernel -lavl -lumem -lslsolkerncompat -ldl -lcrypto

SSL_LIBS=	-lssl -lcrypto

LIBL=		-ll
LIBZ=		-lz
THREAD_LIBS=	-pthread
LIBAIO=		-laio

CURSES_LIBS=	$$(ncurses5-config --libs 2>/dev/null || echo -lncurses)
CURSES_INCLUDES=$$(ncurses5-config --cflags 2>/dev/null)

OSTYPE:=	$(shell uname)

# global file-specific settings
$(call ADD_FILE_CFLAGS,${PFL_BASE}/crc.c,				${COPT})
$(call ADD_FILE_CFLAGS,${PFL_BASE}/parity.c,				${COPT})
$(call ADD_FILE_CFLAGS,${PFL_BASE}/opt-misc.c,				${COPT})

# gcrc module may be disabled so ensure it exists.
ifneq ($(wildcard ${GCRC_BASE}/crc32c_sse4.cc),)
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/crc32c_sse4.cc,			${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/interface.cc,			${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/multiword_128_64_gcc_amd64_sse2.cc,	${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/multiword_64_64_cl_i386_mmx.cc,	${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/multiword_64_64_gcc_amd64_asm.cc,	${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/multiword_64_64_gcc_i386_mmx.cc,	${COPT})
$(call ADD_FILE_CFLAGS,${GCRC_BASE}/multiword_64_64_intrinsic_i386_mmx.cc,${COPT})
endif

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

ifeq ($(shell ${MINVER} ${LEXVER} 2.5.35 && echo 1),1)
  LIBL=		-lfl
  PCPP_FLAGS+=	-H yytext
endif

# system-specific settings

ifeq (${OSTYPE},Linux)
  LIBRT=	-lrt
  LIBDL=	-ldl
  LIBACL=	-lacl
  DEFINES+=	-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
  DEFINES+=	-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq (${OSTYPE},Darwin)
  DEFINES+=	-D_DARWIN_C_SOURCE -D_DARWIN_FEATURE_64_BIT_INODE
  DEFINES+=	-DHAVE_NO_POLL_DEV
  INCLUDES+=	-I/opt/local/include
  CFLAGS+=	-Wno-deprecated-declarations
  THREAD_LIBS=	-lpthread
endif

ifeq (${OSTYPE},OpenBSD)
  DEFINES+=	-D_BSD_SOURCE
endif

ifeq (${OSTYPE},SunOS)
  DEFINES+=	-D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1 -D__EXTENSIONS__
  DEFINES+=	-D_POSIX_PTHREAD_SEMANTICS -DNAME_MAX=255
  LDFLAGS+=	-lxnet -lsocket
  SSL_LIBS+=	-L/opt/local/lib
  SSL_INCLUDES=	-I/opt/local/include
  LEX=		flex
endif

ifeq (${LD},ld)
  LD=		cc
endif

-include ${ROOTDIR}/mk/local.mk
