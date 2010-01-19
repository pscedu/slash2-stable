# $Id$

-include ${ROOTDIR}/mk/local.mk

_TSRCS=			$(foreach fn,${SRCS},$(realpath ${fn}))

_TOBJS=			$(patsubst %.c,%.o,$(filter %.c,${_TSRCS}))
_TOBJS+=		$(patsubst %.y,%.o,$(filter %.y,${_TSRCS}))
_TOBJS+=		$(patsubst %.l,%.o,$(filter %.l,${_TSRCS}))
OBJS=			$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))

_TSUBDIRS=		$(foreach dir,${SUBDIRS},$(realpath ${dir}))

_LEXINTM=		$(patsubst %.l,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.l,${_TSRCS}))))
_YACCINTM=		$(patsubst %.y,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.y,${_TSRCS}))))
_C_SRCS=		$(filter %.c,${_TSRCS}) ${_YACCINTM} ${_LEXINTM}

OBJDIR=			${CURDIR}/obj
DEPEND_FILE=		${OBJDIR}/.depend

LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/conn.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/handlers.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/poll.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-lock.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-prim.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-tcpip.c

LNET_LIB_SRCS+=		${LNET_BASE}/lnet/acceptor.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/api-errno.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/api-ni.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/config.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-eq.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-md.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-me.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-move.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lib-msg.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/lo.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/peer.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/router.c
LNET_LIB_SRCS+=		${LNET_BASE}/lnet/router_proc.c

LNET_PTLLND_SRCS+=	${LNET_BASE}/ulnds/ptllnd/ptllnd.c
LNET_PTLLND_SRCS+=	${LNET_BASE}/ulnds/ptllnd/ptllnd_cb.c

PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/connection.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/events.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/export.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/import.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/nb.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rsx.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/service.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/util.c

_TINCLUDES=		$(filter-out -I%,${INCLUDES}) $(patsubst %,-I%,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${INCLUDES})), $(realpath ${dir})))

CFLAGS+=		${DEFINES} ${_TINCLUDES}
TARGET?=		${PROG} ${LIBRARY}

EXTRACT_INCLUDES=	perl -ne 'print $$& while /-I\S+\s?/gc'
EXTRACT_DEFINES=	perl -ne 'print $$& while /-D\S+\s?/gc'
EXTRACT_CFLAGS=		perl -ne 'print $$& while /-[^ID]\S+\s?/gc'

ifneq ($(filter fuse,${MODULES}),)
CFLAGS+=	${FUSE_CFLAGS}
DEFINES+=	${FUSE_DEFINES}
LDFLAGS+=	${FUSE_LIBS}
MODULES+=	fuse-hdrs
endif

ifneq ($(filter fuse-hdrs,${MODULES}),)
DEFINES+=	-DFUSE_USE_VERSION=${FUSE_VERSION}
INCLUDES+=	${FUSE_INCLUDES}
endif

ifneq ($(filter zfs,${MODULES}),)
INCLUDES+=	-I${ZFS_BASE}
LDFLAGS+=	${ZFS_LIBS}
MODULES+=	z
endif

ifneq ($(filter lnet,${MODULES}),)
SRCS+=		${LNET_CFS_SRCS}
SRCS+=		${LNET_LIB_SRCS}
MODULES+=	lnet-hdrs lnet-nid
endif

ifneq ($(filter lnet-hdrs,${MODULES}),)
INCLUDES+=	-I${LNET_BASE}/include
endif

ifneq ($(filter lnet-nid,${MODULES}),)
SRCS+=		${LNET_BASE}/libcfs/nidstrings.c
endif

ifneq ($(filter pthread,${MODULES}),)
LDFLAGS+=	${THREAD_LIBS}
DEFINES+=	-DHAVE_LIBPTHREAD
endif

ifneq ($(filter curses,${MODULES}),)
LDFLAGS+=	${LIBCURSES}
endif

ifneq ($(filter z,${MODULES}),)
LDFLAGS+=	${LIBZ}
endif

ifneq ($(filter l,${MODULES}),)
LDFLAGS+=	${LIBL}
endif

# OBJDIR is added to .c below since lex/yacc intermediate files get generated there.
vpath %.y $(sort $(dir $(filter %.y,${_TSRCS})))
vpath %.l $(sort $(dir $(filter %.l,${_TSRCS})))
vpath %.c $(sort $(dir $(filter %.c,${_TSRCS})) ${OBJDIR})

all: recurse-all
	@if ${NOTEMPTY} "${TARGET}"; then						\
		if ${NOTEMPTY} "${SUBDIRS}"; then					\
			echo "===> ${DIRPREFIX:+.}";					\
		fi;									\
		${MAKE} -s ${DEPEND_FILE};						\
		MAKEFILES=${DEPEND_FILE} ${MAKE} ${TARGET};				\
	fi

.SUFFIXES:

${OBJDIR}:
	mkdir -p $@

${OBJDIR}/$(notdir %.c) : %.l | ${OBJDIR}
	${LEX} ${LFLAGS} $(realpath $<) > $@

${OBJDIR}/$(notdir %.c) : %.y | ${OBJDIR}
	${YACC} ${YFLAGS} -o $@ $(realpath $<)

${OBJDIR}/$(notdir %.o) : %.c | ${OBJDIR}
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst			\
	    ../,,$(subst //,/,$(subst $(realpath					\
	    ${ROOTDIR})/,,$(realpath $<)))))))_CFLAGS} -I$(dir $<) $(realpath $<) -c -o $@

${OBJDIR}/$(notdir %.E) : %.c | ${OBJDIR}
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst			\
	    ../,,$(subst //,/,$(subst $(realpath					\
	    ${ROOTDIR})/,,$(realpath $<)))))))_CFLAGS} -I$(dir $<) $(realpath $<) -E -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ ${OBJS}

recurse-%:
	@for i in ${_TSUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/			\
		    $(patsubst recurse-%,%,$@)) || exit 1;				\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done

# empty but overrideable
install-hook:

# XXX use install(1)
install: recurse-install install-hook
	@if [ -n "${LIBRARY}" ]; then							\
		${ECHORUN} mkdir -p ${INSTALLDIR}/lib;					\
		${ECHORUN} cp -pf ${LIBRARY} ${INSTALLDIR}/lib;				\
	fi
	@if [ -n "${PROG}" ]; then							\
		${ECHORUN} mkdir -p ${INSTALLDIR}/bin;					\
		${ECHORUN} cp -pf ${PROG} ${INSTALLDIR}/bin;				\
	fi
	@if ${NOTEMPTY} "${HEADERS}"; then						\
		for i in "${HEADERS}"; do						\
			if [ x"$${i%/*}" = x"$$i" ]; then				\
				_dir=${INSTALLDIR}/include/$${i%/*};			\
			else								\
				_dir=${INSTALLDIR}/include;				\
			fi;								\
			${ECHORUN} mkdir -p $$_dir;					\
			${ECHORUN} cp -rfp $$i $$_dir;					\
		done;									\
	fi

${DEPEND_FILE}: ${_C_SRCS} | ${OBJDIR}
	@if ${NOTEMPTY} "${_C_SRCS}"; then						\
		${ECHORUN} ${MKDEP} -D ${OBJDIR} -f ${DEPEND_FILE} ${LIBC_INCLUDES}	\
		    ${_TINCLUDES} ${DEFINES} ${_C_SRCS};				\
	fi
	@if [ -n "${PROG}" ]; then							\
		echo -n "${PROG}:" >> ${DEPEND_FILE};					\
		${LIBDEP} ${LDFLAGS} ${LIBDEP_ADD} >> ${DEPEND_FILE};			\
	fi

clean: recurse-clean
	@if ${NOTEMPTY} "${SUBDIRS}"; then						\
		echo "<=== ${CURDIR}";							\
	fi
	rm -rf ${OBJDIR}
	rm -f ${PROG} ${LIBRARY} TAGS cscope.out core.[0-9]*

lint: recurse-lint ${_C_SRCS}
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		${ECHORUN} ${LINT} ${_TINCLUDES} ${DEFINES} ${_C_SRCS};			\
	fi

listsrcs: recurse-listsrcs
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		echo "${_TSRCS}";							\
	fi

test: recurse-test ${TARGET}
	@if [ -n "${PROG}" ]; then							\
		echo "./${PROG}";							\
		./${PROG} || exit 1;							\
	fi

hdrclean:
	${HDRCLEAN} */*.[clyh]

# empty but overrideable
build-prereq:

build: build-prereq
	${MAKE} clean && ${MAKE}

qbuild:
	@${MAKE} build >/dev/null

copyright: recurse-copyright
	@if ${NOTEMPTY} "${_TSRCS}"; then						\
		${ECHORUN} ${ROOTDIR}/tools/gencopyright.sh ${_TSRCS};			\
	fi
	@find . -mindepth 2 -name '*.h' | xargs -r ${ECHORUN} ${ROOTDIR}/tools/gencopyright.sh

#CS_ARGS+=-s${APP_BASE}
#ET_ARGS+="${APP_BASE}"

ifdef SLASH_BASE
CS_ARGS+=-s${SLASH_BASE}
ET_ARGS+="${SLASH_BASE}"

CS_ARGS+=-s${ZFS_BASE}/zfs-fuse
ET_ARGS+="${ZFS_BASE}/zfs-fuse"
endif

ifdef ZEST_BASE
CS_ARGS+=-s${ZEST_BASE}
ET_ARGS+="${ZEST_BASE}"
endif

cscope cs: recurse-cs
	cscope -Rb ${CS_ARGS} -s${PFL_BASE} -s${LNET_BASE}

etags: recurse-etags
	find ${ET_ARGS} ${PFL_BASE} ${PFL_BASE} -name \*.[chly] | xargs etags

printenv:
	@env
