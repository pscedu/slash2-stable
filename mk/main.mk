# $Id$
# %PSC_START_COPYRIGHT%
# -----------------------------------------------------------------------------
# Copyright (c) 2006-2012, Pittsburgh Supercomputing Center (PSC).
#
# Permission to use, copy, and modify this software and its documentation
# without fee for personal use or non-commercial use within your organization
# is hereby granted, provided that the above copyright notice is preserved in
# all copies and that the copyright and this permission notice appear in
# supporting documentation.  Permission to redistribute this software to other
# organizations or individuals is not permitted without the written permission
# of the Pittsburgh Supercomputing Center.  PSC makes no representations about
# the suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
# -----------------------------------------------------------------------------
# %PSC_END_COPYRIGHT%

CROOTDIR=		$(realpath ${ROOTDIR})

STRIPROOTDIR=		$(subst ${CROOTDIR}/,,$1)
PATH_NAMIFY=		$(subst .,_,$(subst -,_,$(subst /,_,$1)))
FILE_CFLAGS_VARNAME=	$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(abspath $1)))_CFLAGS
FILE_PCPP_FLAGS_VARNAME=$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(abspath $1)))_PCPP_FLAGS
FILE_CFLAGS=		${$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(realpath $1)))_CFLAGS}
FILE_PCPP_FLAGS=	${$(call PATH_NAMIFY,$(call STRIPROOTDIR,$(realpath $1)))_PCPP_FLAGS}
ADD_FILE_CFLAGS=	$(shell if ! [ -f "$(abspath $1)" ]; then echo "ADD_FILE_CFLAGS: no such file: $(abspath $1)" >&2; fi )\
			$(eval $(call FILE_CFLAGS_VARNAME,$1)+=$2)

FORCE_INST?=		0

OBJDIR=			${OBJBASE}/psc.obj${CURDIR}
DEPEND_FILE=		${OBJDIR}/.depend

include ${ROOTDIR}/mk/defs.mk
include ${ROOTDIR}/mk/pickle.mk

_TSRCS=			$(sort $(foreach fn,${SRCS},$(realpath ${fn})))
_TSRC_PATH=		$(shell perl -Wle 'my @a; push @a, shift; for my $$t (sort @ARGV) {	\
			  next if grep { $$t =~ /^\Q$$_\E/ } @a; print $$t; push @a, $$t } '	\
			  $(foreach dir,. ${SRC_PATH},$(realpath ${dir})))

_TOBJS=			$(patsubst %.c,%.o,$(filter %.c,${_TSRCS}))
_TOBJS+=		$(patsubst %.y,%.o,$(filter %.y,${_TSRCS}))
_TOBJS+=		$(patsubst %.l,%.o,$(filter %.l,${_TSRCS}))
OBJS=			$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))

_TSUBDIRS=		$(foreach dir,${SUBDIRS},$(realpath ${dir}))

_LEXINTM=		$(patsubst %.l,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.l,${_TSRCS}))))
_YACCINTM=		$(patsubst %.y,%.c,$(addprefix ${OBJDIR}/,$(notdir $(filter %.y,${_TSRCS}))))
_C_SRCS=		$(filter %.c,${_TSRCS}) ${_YACCINTM} ${_LEXINTM}

LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/conn.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/handlers.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/poll.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd.c
LNET_SOCKLND_SRCS+=	${LNET_BASE}/ulnds/socklnd/usocklnd_cb.c

LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/debug.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-lock.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-prim.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-tcpip.c
LNET_CFS_SRCS+=		${LNET_BASE}/libcfs/user-tcpip-ssl.c

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

PSCRPC_SRCS+=		${PFL_BASE}/connection.c
PSCRPC_SRCS+=		${PFL_BASE}/events.c
PSCRPC_SRCS+=		${PFL_BASE}/export.c
PSCRPC_SRCS+=		${PFL_BASE}/import.c
PSCRPC_SRCS+=		${PFL_BASE}/nb.c
PSCRPC_SRCS+=		${PFL_BASE}/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/rsx.c
PSCRPC_SRCS+=		${PFL_BASE}/service.c
PSCRPC_SRCS+=		${PFL_BASE}/util.c

_TINCLUDES=		$(filter-out -I%,${INCLUDES}) $(patsubst %,-I%,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${INCLUDES})), $(realpath ${dir})))

CFLAGS+=		${DEFINES} ${_TINCLUDES}
TARGET?=		$(sort ${PROG} ${LIBRARY} ${TEST})
PROG?=			${TEST}

EXTRACT_INCLUDES=	perl -ne 'print $$& while /-I\S+\s?/gc'
EXTRACT_DEFINES=	perl -ne 'print $$& while /-D\S+\s?/gc'
EXTRACT_CFLAGS=		perl -ne 'print $$& while /-[^ID]\S+\s?/gc'

# Pre-modules processing

ifneq ($(filter ${PFL_BASE}/%.c,${SRCS}),)
  MODULES+=	pfl
endif

# Process modules

ifneq ($(filter pscfs,${MODULES}),)
  MODULES+=	pscfs-hdrs

  ifndef PICKLE_HAVE_LP64
    PSCFS_SRCS+=${PFL_BASE}/listcache.c
    PSCFS_SRCS+=${PFL_BASE}/pool.c
    PSCFS_SRCS+=${PFL_BASE}/random.c
  endif

  SRCS+=	${PSCFS_SRCS}
  ifdef PICKLE_HAVE_FUSE
    MODULES+=	fuse
  else ifdef PICKLE_HAVE_NNPFS
    MODULES+=	nnpfs
  else ifdef PICKLE_HAVE_DOKAN
    MODULES+=	dokan
  else
    $(error no pscfs support available)
  endif
endif

ifneq ($(filter pscfs-hdrs,${MODULES}),)
  ifdef PICKLE_HAVE_FUSE
    MODULES+=	fuse-hdrs
  else ifdef PICKLE_HAVE_NNPFS
    MODULES+=	nnpfs-hdrs
  else ifdef PICKLE_HAVE_DOKAN
    MODULES+=	dokan-hdrs
  else
    $(error no pscfs support available)
  endif
endif

ifneq ($(filter fuse,${MODULES}),)
  CFLAGS+=	${FUSE_CFLAGS}
  LDFLAGS+=	${FUSE_LIBS}
  MODULES+=	fuse-hdrs
endif

ifneq ($(filter fuse-hdrs,${MODULES}),)
  DEFINES+=	-DFUSE_USE_VERSION=${FUSE_VERSION} ${FUSE_DEFINES}
  INCLUDES+=	${FUSE_INCLUDES}
endif

ifneq ($(filter zfs,${MODULES}),)
  INCLUDES+=	-I${ZFS_BASE}/src
  SRC_PATH+=	${ZFS_BASE}/src
  LDFLAGS+=	${ZFS_LIBS}
  MODULES+=	z
endif

ifneq ($(filter rpc,${MODULES}),)
  SRCS+=	${PSCRPC_SRCS}
  MODULES+=	lnet
endif

ifneq ($(filter lnet,${MODULES}),)
  SRCS+=	${PFL_BASE}/iostats.c
  SRCS+=	${LNET_SOCKLND_SRCS}
  SRCS+=	${LNET_CFS_SRCS}
  SRCS+=	${LNET_LIB_SRCS}
  MODULES+=	lnet-hdrs lnet-nid ssl
endif

ifneq ($(filter lnet-hdrs,${MODULES}),)
  INCLUDES+=	-I${LNET_BASE}/include
  INCLUDES+=	-I${LNET_BASE}
  SRC_PATH+=	${LNET_BASE}
endif

ifneq ($(filter lnet-nid,${MODULES}),)
  SRCS+=	${LNET_BASE}/libcfs/nidstrings.c
endif

ifneq ($(filter pthread,${MODULES}),)
  LDFLAGS+=	${THREAD_LIBS}
  DEFINES+=	-DHAVE_LIBPTHREAD
  MODULES+=	rt
endif

ifneq ($(filter ssl,${MODULES}),)
  LDFLAGS+=	-lssl -lcrypto
endif

ifneq ($(filter rt,${MODULES}),)
  LDFLAGS+=	${LIBRT}
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

ifneq ($(filter m,${MODULES}),)
  LDFLAGS+=	-lm
endif

ifneq ($(filter pfl,${MODULES}),)
  MODULES+=	pfl-hdrs str
  SRCS+=	${PFL_BASE}/alloc.c
  SRCS+=	${PFL_BASE}/init.c
  SRCS+=	${PFL_BASE}/log.c

  ifneq (${DEBUG},0)
    SRCS+=	${PFL_BASE}/dbgutil.c
    SRCS+=	${PFL_BASE}/printhex.c
  endif

 ifneq ($(filter pthread,${MODULES}),)
   MODULES+=	numa
 endif
endif

ifneq ($(filter pfl-hdrs,${MODULES}),)
  INCLUDES+=	-I${PFL_BASE}/include
  SRC_PATH+=	${PFL_BASE}
endif

ifneq ($(filter mpi,${MODULES}),)
  CC=		${MPICC}
  DEFINES+=	-DMPI
  LDFLAGS+=	${MPILIBS}
endif

ifneq ($(filter qk,${MODULES}),)
  CC=		${QKCC}
  DEFINES+=	-DQK
#INCLUDES+=	-I/opt/xt-mpt/default/mpich2-64/P2/include
#LDFLAGS+=	-L/opt/xt-mpt/default/mpich2-64/P2/lib -lmpich
endif

ifneq ($(filter zcc,${MODULES}),)
  CC=		ZINCPATH=${ZEST_BASE}/intercept/include \
		ZLIBPATH=${ZEST_BASE}/client/linux-mt ${ZEST_BASE}/scripts/zcc
endif

ifneq ($(filter numa,${MODULES}),)
  LDFLAGS+=	${NUMA_LIBS}
endif

ifneq ($(filter gcrypt,${MODULES}),)
  CFLAGS+=	${GCRYPT_CFLAGS}
  DEFINES+=	${GCRYPT_DEFINES}
  LDFLAGS+=	${GCRYPT_LIBS}
  INCLUDES+=	${GCRYPT_INCLUDES}
endif

ifneq ($(filter aio,${MODULES}),)
  LDFLAGS+=	${LIBAIO}
endif

ifneq ($(filter ctl,${MODULES}),)
  SRCS+=	${PFL_BASE}/ctlsvr.c
  SRCS+=	${PFL_BASE}/netutil.c
  DEFINES+=	-DPFL_CTL
endif

ifneq ($(filter ctlcli,${MODULES}),)
  SRCS+=	${PFL_BASE}/ctlcli.c
endif

ifneq ($(filter sgio,${MODULES}),)
  ifdef PICKLE_HAVE_SGIO
    SRCS+=	${ZEST_BASE}/sgio/sg.c
    SRCS+=	${ZEST_BASE}/sgio/sg_async.c
  endif
endif

ifneq ($(filter sqlite,${MODULES}),)
  LDFLAGS+=	-lsqlite3
endif

ifneq ($(filter readline,${MODULES}),)
  LDFLAGS+=	-lreadline
endif

ifneq ($(filter str,${MODULES}),)
  SRCS+=	${STR_SRCS}
endif

ifneq ($(filter futimens,${MODULES}),)
  SRCS+=	${FUTIMENS_SRCS}
endif

ifneq ($(filter random,${MODULES}),)
  SRCS+=	${RND_SRCS}
endif

ifneq ($(filter barrier,${MODULES}),)
  SRCS+=	${BARRIER_SRCS}
endif

# Post-modules processing

ifneq ($(filter ${PFL_BASE}/pthrutil.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/vbitmap.c
  SRCS+=	${PFL_BASE}/log.c
  SRCS+=	${PFL_BASE}/thread.c
endif

ifneq ($(filter ${PFL_BASE}/thread.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/lockedlist.c
  SRCS+=	${PFL_BASE}/subsys.c
  SRCS+=	${PFL_BASE}/waitq.c
  SRCS+=	${CLOCK_SRCS}
endif

ifneq ($(filter ${PFL_BASE}/subsys.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/dynarray.c
endif

ifneq ($(filter ${PFL_BASE}/log.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/alloc.c
endif

ifneq ($(filter ${PFL_BASE}/alloc.c,${SRCS}),)
  ifneq (${DEBUG},0)
    SRCS+=	${PFL_BASE}/hashtbl.c
  endif
endif

ifneq ($(filter ${PFL_BASE}/hashtbl.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/lockedlist.c
endif

ifneq ($(filter ${PFL_BASE}/lockedlist.c,${SRCS}),)
  SRCS+=	${PFL_BASE}/list.c
endif

# OBJDIR is added to .c below since lex/yacc intermediate files get
# generated there.
vpath %.y $(sort $(dir $(filter %.y,${_TSRCS})))
vpath %.l $(sort $(dir $(filter %.l,${_TSRCS})))
vpath %.c $(sort $(dir $(filter %.c,${_TSRCS})) ${OBJDIR})

all: recurse-all all-hook
	@for i in ${SRCS}; do						\
		[ -n "$$i" ] || continue;				\
		if ! [ -e "$$i" ]; then					\
			echo "$$i does not exist" >&2;			\
			exit 1;						\
		fi;							\
	done
	@if ${NOTEMPTY} "${TARGET}"; then				\
		${MKDIRS} -m 775 ${OBJDIR};				\
		${MAKE} ${TARGET};					\
	fi

all-hook:

.SUFFIXES:

.PRECIOUS: ${OBJDIR}/$(notdir %.d)

# XXX this doesn't seem to work as advertised
.SILENT: ${OBJDIR}/$(notdir %.d)

${OBJDIR}/$(notdir %.o) : %.c
	${PCPP} ${PCPP_FLAGS} $(call FILE_PCPP_FLAGS,$<) $(realpath $<	\
	    ) | ${CC} -x c ${CFLAGS} $(call FILE_CFLAGS,$<) $(		\
	    ) -I$(dir $<) -I. - -c -o $@ -MD -MP

${OBJDIR}/$(notdir %.E) : %.c
	${CC} ${CFLAGS} $(call FILE_CFLAGS,$<) -I$(dir $<) -I. $(realpath $<) -E -o $@

${OBJDIR}/$(notdir %.c) : %.l
	echo "${LEX} ${LFLAGS} $(realpath $<) > $@"
	${LEX} ${LFLAGS} $(realpath $<) > $@

#	$(eval $$(call FILE_PCPP_FLAGS_VARNAME,$@)+=$$(call FILE_PCPP_FLAGS,$<))
#	$(eval $$(call FILE_CFLAGS_VARNAME,$@)+=$$(call FILE_CFLAGS,$<))

${OBJDIR}/$(notdir %.c) : %.y
	${ECHORUN} ${YACC} ${YFLAGS} -o $@ $(realpath $<)

ifdef PROG
${PROG}: ${OBJS}
	${CC} -o $@ $(sort ${OBJS}) ${LDFLAGS}
	@printf "%s" "${PROG}:" > ${DEPEND_FILE}
	@${LIBDEP} ${LDFLAGS} ${LIBDEP_ADD} >> ${DEPEND_FILE}
endif

ifdef LIBRARY
${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ $(sort ${OBJS})
endif

recurse-%:
	@if [ $(words ${SUBDIRS}) -ne					\
	      $(words $(sort ${SUBDIRS})) ]; then			\
		echo "duplicate in SUBDIRS" >&2;			\
		false;							\
	fi
	@for i in ${_TSUBDIRS}; do					\
		echo "===> $$i $(patsubst recurse-%,%,$@)";		\
		if [ "${PSC_MAKE_STATUS}" ]; then			\
			printf " %s" "$${i#${CROOTDIR}/} $(		\
			    )$(patsubst recurse-%,%,$@)" >&2;		\
			${CLEAR_EOL} >&2 || true;			\
			printf "\r" >&2;				\
		fi;							\
		(cd $$i && SUBDIRS= ${MAKE}				\
		    $(patsubst recurse-%,%,$@)) || exit 1;		\
	done
	@if ${NOTEMPTY} "${_TSUBDIRS}"; then				\
		echo "<=== ${CURDIR}";					\
		if [ "${PSC_MAKE_STATUS}" ]; then			\
			${CLEAR_EOL} >&2 || true;			\
		fi;							\
	fi

# empty but overrideable
install-hook:

install: recurse-install install-hook
	@if [ -n "${LIBRARY}" ]; then					\
		${INST} ${LIBRARY} ${INST_LIBDIR}/;			\
	fi
	@# skip programs part of test suites
	@if ${NOTEMPTY} "${PROG}${BIN}" &&				\
	    [ -z "$(findstring /tests/,${CURDIR})" -a			\
	      -z "$(findstring /utils/,${CURDIR})" -o			\
	      "${FORCE_INST}" -eq 1 ]; then				\
		dir="${INST_BINDIR}";					\
		man="${MAN}";						\
		if [ -n "${MAN}" -a x"$${man%.8}" != x"${MAN}" ]; then	\
			dir="${INST_SBINDIR}";				\
		fi;							\
		if ${NOTEMPTY} "${INSTDIR}"; then			\
			dir="${INSTDIR}";				\
		fi;							\
		${INST} -m 555 ${PROG} ${BIN} "$$dir"/;			\
		for bin in ${BIN}; do					\
			if head -1 $$bin | grep -aq perl; then		\
				${ECHORUN} perl -i -Wpe			\
				    's{^# use lib qw\(%INST_PLMODDIR%\);$$}$(	\
				     ){use lib qw(${INST_PLMODDIR});}'	\
				     $$dir/$$bin;			\
			fi;						\
		done;							\
	fi
	@if ${NOTEMPTY} "${MAN}"; then					\
		for i in ${MAN}; do					\
			dir=${INST_MANDIR}/man$${i##*.};		\
			${INST} -m 444 $$i "$$dir"/;			\
		done;							\
	fi
	@if ${NOTEMPTY} "${HEADERS}"; then				\
		for i in ${HEADERS}; do					\
			base=$$(basename "$$i");			\
			dir=${INST_INCDIR}/$$(dirname "$$i");		\
			${INST} -m 444 $$i "$$dir"/;			\
		done;							\
	fi
	@if ${NOTEMPTY} "${PLMODS}"; then				\
		for i in ${PLMODS}; do					\
			base=$$(basename "$$i");			\
			dir=${INST_PLMODDIR}/$$(dirname "$$i");		\
			${INST} -m 444 $$i $$dir/;			\
		done;							\
	fi

clean-hook:

clean-core:
	${RM} -rf ${OBJDIR}
	${RM} -f ${PROG} ${LIBRARY} core.[0-9]* *.core

clean: recurse-clean clean-core clean-hook

distclean: recurse-distclean clean-core
	${RM} -f TAGS cscope.*out

lint: recurse-lint ${_C_SRCS}
	@if ${NOTEMPTY} "${_TSRCS}"; then				\
		${ECHORUN} ${LINT} ${_TINCLUDES} ${DEFINES} ${_C_SRCS};	\
	fi

listsrcs: recurse-listsrcs
	@if ${NOTEMPTY} "${_TSRCS}"; then				\
		echo "${_TSRCS}";					\
	fi

test-hook:
	@if [ -n "${TEST}" ]; then					\
		echo "./${TEST}";					\
		./${TEST} ${TESTOPTS} || exit 1;			\
	fi

runtest: recurse-runtest test-hook

maketest: recurse-maketest ${TEST}

test:
	@${MAKE} maketest && ${MAKE} runtest

hdrclean:
	${HDRCLEAN} */*.[clyh]

# empty but overrideable
regen-hook:

regen: recurse-regen regen-hook

build:
	${MAKE} clean && ${MAKE} regen && ${MAKE} all

copyright:
	find . -type f \( $(foreach ign,${COPYRIGHT_PATS},-name ${ign} -o) -false \) $(	\
	    ) -exec ${ECHORUN} ${ROOTDIR}/tools/gencopyright.sh {} \;

doc: recurse-doc
	@if ${NOTEMPTY} "${MAN}"; then						\
		${ECHORUN} ${MDPROC} $$(echo ${MAN} $(				\
		    ) $$([ -e ${PROG}.[0-9] ] && echo ${PROG}.[0-9]) $(		\
		    ) $$([ -e ${LIBRARY}.[0-9] ] && echo ${LIBRARY}.[0-9]) |	\
		    tr ' ' '\n' | sort -u);					\
	fi

printvar-%:
	@echo ${$(patsubst printvar-%,%,$@)}

cscope cs: recurse-cs
	cscope -Rbq $(addprefix -s,$(filter-out ${CURDIR},${_TSRC_PATH}))

etags et: recurse-etags
	find . ${_TSRC_PATH} -name \*.[chly] | xargs etags

printenv:
	@env | sort

-include ${DEPEND_FILE}
-include ${OBJDIR}/*.d
