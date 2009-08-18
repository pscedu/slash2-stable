# $Id$

-include ${ROOTDIR}/mk/local.mk

_TOBJS=		$(patsubst %.c,%.o,$(filter %.c,${SRCS}))
_TOBJS+=	$(patsubst %.y,%.o,$(filter %.y,${SRCS}))
_TOBJS+=	$(patsubst %.l,%.o,$(filter %.l,${SRCS}))
OBJS=		$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))

_LEXINTM=	$(patsubst %.l,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.l,${SRCS}))))
_YACCINTM=	$(patsubst %.y,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.y,${SRCS}))))
CLEANFILES+=	$(patsubst %.y,%.h,$(notdir $(filter %.y,${SRCS})))
_C_SRCS=	$(filter %.c,${SRCS}) ${_YACCINTM} ${_LEXINTM}
ECHORUN=	echorun() { echo "$$@"; "$$@" }; echorun

LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/conn.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/handlers.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/poll.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/nidstrings.c
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
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/service.c

CFLAGS+=		${INCLUDES} ${DEFINES}
TARGET?=	        ${PROG} ${LIBRARY}
OBJDIR=			${CURDIR}/obj

vpath %.y $(sort $(dir $(filter %.y,${SRCS})))
vpath %.l $(sort $(dir $(filter %.l,${SRCS})))
vpath %.c $(sort $(dir $(filter %.c,${SRCS})) ${OBJDIR})

all: recurse-all ${TARGET}

recurse-all:
	@# XXX: factor recursion
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ all) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done

.SUFFIXES:

${OBJDIR}:
	mkdir -p $@

${OBJDIR}/$(notdir %.c) : %.l | ${OBJDIR}
	${LEX} ${LFLAGS} $(realpath $<) > $@

${OBJDIR}/$(notdir %.c) : %.y | ${OBJDIR}
	${YACC} ${YFLAGS} -o $@ $(realpath $<)

${OBJDIR}/$(notdir %.o) : %.c | ${OBJDIR}
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst			\
	    ../,,$(subst //,/,$<)))))_CFLAGS} -c $(realpath $<) -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ ${OBJS}

recurse-install:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	@if [ -n "${LIBRARY}" ]; then							\
		mkdir -p ${INSTALLDIR}/lib;						\
		echo cp -pf ${LIBRARY} ${INSTALLDIR}/lib;				\
		cp -pf ${LIBRARY} ${INSTALLDIR}/lib;					\
	fi
	@if [ -n "${PROG}" ]; then							\
		mkdir -p ${INSTALLDIR}/bin;						\
		echo cp -pf ${PROG} ${INSTALLDIR}/bin;					\
		cp -pf ${PROG} ${INSTALLDIR}/bin;					\
	fi
	@if ${NOTEMPTY} "${HEADERS}"; then						\
		for i in "${HEADERS}"; do						\
			if [ x"$${i%/*}" = x"$$i" ]; then				\
				_dir=${INSTALLDIR}/include/$${i%/*};			\
			else								\
				_dir=${INSTALLDIR}/include;				\
			fi;								\
			mkdir -p $$_dir;						\
			echo cp -rfp $$i $$_dir;					\
			cp -rfp $$i $$_dir;						\
		done;									\
	fi

depend: ${_C_SRCS}
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	@if ${NOTEMPTY} "${_C_SRCS}"; then						\
		echo "${MKDEP} ${INCLUDES} ${DEFINES} ${_C_SRCS}";			\
		${MKDEP} ${INCLUDES} ${DEFINES} ${_C_SRCS};				\
	fi
	@if [ -n "${PROG}" ]; then							\
		echo -n "${PROG}:" >> .depend;						\
		perl ${ROOTDIR}/tools/libdep.pl ${LDFLAGS} >> .depend;			\
	fi

clean:
	@# Check existence of files to catch errors such as SRCS+=file.y instead of file.c
	@for i in ${SRCS}; do								\
		test -f $$i || { echo "file does not exist: $$i" >&2; exit 1; };	\
	done
	rm -rf ${OBJS} ${PROG} ${LIBRARY} $(addprefix ${OBJDIR}/,${CLEANFILES})		\
	    ${_YACCINTM} ${_LEXINTM} .depend* TAGS cscope.out core.[0-9]*
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done

lint:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	@if ${NOTEMPTY} "${SRCS}"; then							\
		echo "${LINT} ${INCLUDES} ${DEFINES} ${SRCS}";				\
		${LINT} ${INCLUDES} ${DEFINES} ${SRCS} || true;				\
	fi

listsrcs:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	@if ${NOTEMPTY} "${SRCS}"; then							\
		echo "${SRCS}";								\
	fi

test: all
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	@if [ -n "${PROG}" ]; then							\
		echo "./${PROG}";							\
		./${PROG} || exit 1;							\
	fi

prereq:

build: prereq
	${MAKE} clean && ${MAKE} depend && ${MAKE}

#CS_ARGS+=-s${APP_BASE}
#ET_ARGS+="${APP_BASE}"

ifdef SLASH_BASE
CS_ARGS+=-s${SLASH_BASE}
ET_ARGS+="${SLASH_BASE}"
endif

ifdef ZEST_BASE
CS_ARGS+=-s${ZEST_BASE}
ET_ARGS+="${ZEST_BASE}"
endif

cscope cs:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	cscope -Rb ${CS_ARGS} -s${PFL_BASE} -s${LNET_BASE}

etags:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> ";							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX};						\
		fi;									\
		echo $$i;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!';			\
		fi;									\
	done
	find ${ET_ARGS} ${PFL_BASE} ${PFL_BASE} -name \*.[chly] | xargs etags

env:
	@env

-include .depend
