# $Id$

-include ${ROOTDIR}/mk/local.mk

OBJS=		$(patsubst %.c,%.o,$(filter %.c,${SRCS}))
OBJS+=		$(patsubst %.y,%.o,$(filter %.y,${SRCS}))
OBJS+=		$(patsubst %.l,%.o,$(filter %.l,${SRCS}))

_YACCINTM=	$(patsubst %.y,%.c,$(filter %.y,${SRCS}))
_LEXINTM=	$(patsubst %.l,%.c,$(filter %.l,${SRCS}))

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
	${CC} ${CFLAGS} -c $< -o $@

${PROG}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

lib: ${LIBRARY}

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

clean:
	rm -rf ${OBJS} ${PROG} ${LIBRARY} ${CLEANFILES} ${_YACCINTM} ${_LEXINTM}	\
	    .depend* tags cscope.out
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
	find ${ROOTDIR}/{lnet-lite,psc_fsutil_libs} ${ET_ARGS} -name \*.[chly] -exec etags -a {} \;

-include .depend
