# $Id$

-include ${ROOTDIR}/mk/local.mk

OBJS=		$(patsubst %.c,%.o,$(filter %.c,${SRCS}))
OBJS+=		$(patsubst %.y,%.o,$(filter %.y,${SRCS}))
OBJS+=		$(patsubst %.l,%.o,$(filter %.l,${SRCS}))

_YACCINTM=	$(patsubst %.y,%.c,$(filter %.y,${SRCS}))
_LEXINTM=	$(patsubst %.l,%.c,$(filter %.l,${SRCS}))

LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/connection.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/pqtimer.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/procapi.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/proclib.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/select.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/table.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/tcplnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/socklnd/sendrecv.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/nidstrings.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-lock.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-prim.c

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

LNET_PTLLND_SRCS+=	${LNET_BASE}/ptllnd/ptllnd.c
LNET_PTLLND_SRCS+=	${LNET_BASE}/ptllnd/ptllnd_cb.c

PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/connection.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/events.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/export.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/import.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/nb.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/psc_rpc/service.c

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

.y.c:
	${YACC} ${YFLAGS} $<

.c.o:
	${CC} ${CFLAGS} ${$(subst .,_,$(subst -,_,$(subst /,_,$(subst ../,,$(subst //,/,$<)))))_CFLAGS} -c $< -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ $^

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
		echo cp -pf ${PROG} ${INSTALLDIR}/bin;				\
		cp -pf ${PROG} ${INSTALLDIR}/bin;					\
	fi
	@if [ -n "${HEADERS}" ]; then							\
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

depend: ${_YACCINTM}
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
	@if [ -n "${SRCS}" ]; then							\
		touch .depend;								\
		echo "${MKDEP} ${INCLUDES} ${DEFINES} ${SRCS}";				\
		${MKDEP} ${INCLUDES} ${DEFINES} ${SRCS};				\
	fi
	@if [ -n "${PROG}" ]; then							\
		echo -n "${PROG}:" >> .depend;						\
		perl ${ROOTDIR}/tools/libdep.pl ${LDFLAGS} >> .depend;			\
		echo >> .depend;							\
	fi

clean:
	rm -rf ${OBJS} ${PROG} ${LIBRARY} ${CLEANFILES} ${_YACCINTM} ${_LEXINTM}	\
	    .depend* tags cscope.out core.[0-9]*
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
	@if [ -n "${SRCS}" ]; then							\
		echo "${LINT} ${INCLUDES} ${DEFINES} ${SRCS}";				\
		${LINT} ${INCLUDES} ${DEFINES} ${SRCS} || true;				\
	fi

listsrcs:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> " >&2;							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX} >&2;					\
		fi;									\
		echo $$i >&2;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!' >&2;			\
		fi;									\
	done
	@if [ -n "${SRCS}" ]; then							\
		echo "${SRCS}";								\
	fi

hdr-sync:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> " >&2;							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX} >&2;					\
		fi;									\
		echo $$i >&2;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!' >&2;			\
		fi;									\
	done
	if [ -z "${PROJECT_BASE}" ]; then						\
		echo "PROJECT_BASE not defined, aborting" >&2;				\
		exit 1;									\
	fi
	@for i in "${SRCS}"; do								\
		sh ${ROOTDIR}/tools/hdr-sync.sh "${PROJECT_BASE}" $$i;			\
	done

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
		echo -n "===> " >&2;							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX} >&2;					\
		fi;									\
		echo $$i >&2;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!' >&2;			\
		fi;									\
	done
	cscope -Rb -s${ROOTDIR}/{lnet-lite,psc_fsutil_libs} ${CS_ARGS}

etags:
	@for i in ${SUBDIRS}; do							\
		echo -n "===> " >&2;							\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo -n ${DIRPREFIX} >&2;					\
		fi;									\
		echo $$i >&2;								\
		(cd $$i && ${MAKE} SUBDIRS= DIRPREFIX=${DIRPREFIX}$$i/ $@) || exit 1;	\
		if [ -n "${DIRPREFIX}" ]; then						\
			echo "<=== ${DIRPREFIX}" | sed 's!/$$!!' >&2;			\
		fi;									\
	done
	find ${ROOTDIR}/{lnet-lite,psc_fsutil_libs} ${ET_ARGS} -name \*.[chly] | xargs etags

-include .depend
