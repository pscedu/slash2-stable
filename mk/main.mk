# $Id$

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
	    .depend tags
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

-include .depend
