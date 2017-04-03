# $Id$
# %ISC_START_LICENSE%
# ---------------------------------------------------------------------
# Copyright 2015-2016, Google, Inc.
# Copyright (c) 2006-2015, Pittsburgh Supercomputing Center (PSC).
# All rights reserved.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
# --------------------------------------------------------------------
# %END_LICENSE%

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

RELCURDIR=		$(call STRIPROOTDIR,${CURDIR}/)
OBJDIR?=		${OBJBASE}/psc.obj/${HOST}${CURDIR}
DEPEND_FILE=		${OBJDIR}/.depend

HOST=			$(word 1,$(subst ., ,$(shell hostname)))

-include ${ROOTDIR}/mk/local.${HOST}.mk
include ${ROOTDIR}/mk/defs.mk
include ${ROOTDIR}/mk/pickle.mk
-include ${ROOTDIR}/mk/local-post.${HOST}.mk

_TSRCS=			$(sort $(foreach fn,${SRCS},$(realpath ${fn})))
_TSRC_PATH=		$(shell perl -Wle 'my @a; push @a, shift; for my $$t (sort @ARGV) {	\
			  next if grep { $$t =~ /^\Q$$_\E/ } @a; print $$t; push @a, $$t } '	\
			  $(foreach dir,. ${SRC_PATH},$(realpath ${dir})))

_TOBJS=			$(patsubst %.c,%.o,$(filter %.c,${_TSRCS}))
_TOBJS+=		$(patsubst %.cc,%.o,$(filter %.cc,${_TSRCS}))
_TOBJS+=		$(patsubst %.y,%.o,$(filter %.y,${_TSRCS}))
_TOBJS+=		$(patsubst %.l,%.o,$(filter %.l,${_TSRCS}))
OBJS=			$(addprefix ${OBJDIR}/,$(notdir ${_TOBJS}))

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

PSCRPC_SRCS+=		${PFL_BASE}/completion.c
PSCRPC_SRCS+=		${PFL_BASE}/connection.c
PSCRPC_SRCS+=		${PFL_BASE}/events.c
PSCRPC_SRCS+=		${PFL_BASE}/export.c
PSCRPC_SRCS+=		${PFL_BASE}/import.c
PSCRPC_SRCS+=		${PFL_BASE}/nb.c
PSCRPC_SRCS+=		${PFL_BASE}/niobuf.c
PSCRPC_SRCS+=		${PFL_BASE}/packgeneric.c
PSCRPC_SRCS+=		${PFL_BASE}/pool.c
PSCRPC_SRCS+=		${PFL_BASE}/rpc_util.c
PSCRPC_SRCS+=		${PFL_BASE}/rpcclient.c
PSCRPC_SRCS+=		${PFL_BASE}/rsx.c
PSCRPC_SRCS+=		${PFL_BASE}/service.c

PSCRPC_SRCS+=		${PFL_BASE}/eqpollthr.c
PSCRPC_SRCS+=		${PFL_BASE}/usklndthr.c

_TINCLUDES=		$(filter-out -I%,${INCLUDES}) $(patsubst %,-I%/,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${INCLUDES})), $(realpath ${dir})))

_EXCLUDES=		$(filter-out -I%,${EXCLUDES}) $(patsubst %,-I%/,$(foreach \
			dir,$(patsubst -I%,%,$(filter -I%,${EXCLUDES})), $(realpath ${dir})))

_TPROG=			$(shell echo ${PROG} | sed 's/:[^:]*//g')
_TSHLIB=		$(shell echo ${SHLIB} | sed 's/:[^:]*//g')

CFLAGS+=		${DEFINES} $(filter-out ${_EXCLUDES},${_TINCLUDES})
TARGET?=		$(sort ${_TPROG} ${LIBRARY} ${TEST} ${DOCGEN} ${_TSHLIB})
PROG?=			${TEST}

EXTRACT_INCLUDES=	perl -ne 'print $$& while /-I\S+\s?/gc'
EXTRACT_DEFINES=	perl -ne 'print $$& while /-D\S+\s?/gc'
EXTRACT_CFLAGS=		perl -ne 'print $$& while /-[^ID]\S+\s?/gc'

# Process modules

#
# XXX Make this dependent on the value of PFL_DEBUG
#
#ifdef PICKLE_HAVE_FSANITIZE
#  FSANITIZE_CFLAGS=	-fsanitize=address -fno-optimize-sibling-calls
#  FSANITIZE_LDFLAGS=	-fsanitize=address
#endif
#

LIBPFL=			-lpfl
ifneq ($(filter pfl-whole,${MODULES}),)
  MODULES+=		pfl
  LIBPFL=		-Wl,-whole-archive -lpfl -Wl,-no-whole-archive
endif

ifneq ($(filter pfl,${MODULES}),)
  MODULES+=	pfl-hdrs str clock pthread
  LDFLAGS+=	-L${PFL_BASE} ${LIBPFL} -lm
  ifdef PICKLE_HAVE_BACKTRACE
    LDFLAGS+=	-rdynamic
  endif
  DEPLIST+=	${PFL_BASE}:libpfl.a
  SRCS+=	${QSORT_R_SRCS}

  ifneq ($(filter pthread,${MODULES}),)
    MODULES+=	numa
  endif
endif

ifneq ($(filter pscfs,${MODULES}),)
  MODULES+=	pscfs-hdrs
  SRCS+=	${PFL_BASE}/fs.c
  SRCS+=	${PSCFS_SRCS}

  ifdef PICKLE_HAVE_FUSE
    MODULES+=	fuse
  else ifdef PICKLE_HAVE_DOKAN
    MODULES+=	dokan
  else
    $(error pscfs was not able to find a suitable backend -- check FUSE installation)
  endif
endif

ifneq ($(filter pscfs-hdrs,${MODULES}),)
  ifdef PICKLE_HAVE_FUSE
    MODULES+=	fuse-hdrs
  else ifdef PICKLE_HAVE_DOKAN
    MODULES+=	dokan-hdrs
  else
    $(error pscfs was not able to find a suitable backend -- check FUSE installation)
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
  DEFINES+=	-DPFL_RPC
  MODULES+=	lnet
endif

ifneq ($(filter ctlcli,${MODULES}),)
  SRCS+=	${PFL_BASE}/ctlcli.c
  MODULES+=	lnet-hdrs
endif

ifneq ($(filter lnet,${MODULES}),)
  SRCS+=	${LNET_SOCKLND_SRCS}
  SRCS+=	${LNET_CFS_SRCS}
  SRCS+=	${LNET_LIB_SRCS}
  MODULES+=	lnet-hdrs lnet-nid ssl
endif

ifneq ($(filter lnet-hdrs,${MODULES}),)
  INCLUDES+=	-I${LNET_BASE}/include
  INCLUDES+=	-I${LNET_BASE}
  INCLUDES+=	${SSL_INCLUDES}
  SRC_PATH+=	${LNET_BASE}
endif

ifneq ($(filter lnet-nid,${MODULES}),)
  SRCS+=	${LNET_BASE}/libcfs/nidstrings.c
endif

ifneq ($(filter ssl,${MODULES}),)
  LDFLAGS+=	${SSL_LIBS}
  INCLUDES+=	${SSL_INCLUDES}
endif

ifneq ($(filter curses,${MODULES}),)
  LDFLAGS+=	${CURSES_LIBS}
  INCLUDES+=	${CURSES_INCLUDES}
endif

ifneq ($(filter z,${MODULES}),)
  LDFLAGS+=	${LIBZ}
endif

ifneq ($(filter l,${MODULES}),)
  LDFLAGS+=	${LIBL}
endif

ifneq ($(filter pthread,${MODULES}),)
  LDFLAGS+=	${THREAD_LIBS}
  DEFINES+=	-DHAVE_LIBPTHREAD
  MODULES+=	rt
endif

ifneq ($(filter acl,${MODULES}),)
  LDFLAGS+=	${LIBACL}
endif

ifneq ($(filter m,${MODULES}),)
  LDFLAGS+=	-lm
endif

ifneq ($(filter pfl-hdrs,${MODULES}),)
  INCLUDES+=	-I${PFL_BASE}/include
  SRC_PATH+=	${PFL_BASE}
endif

ifneq ($(filter mpi,${MODULES}),)
  CC=		${MPICC}
  DEFINES+=	-DHAVE_MPI
  LDFLAGS+=	${MPILIBS}
endif

ifneq ($(filter zcc,${MODULES}),)
  CC=		ZINCPATH=${ZEST_BASE}/intercept/include \
		ZLIBPATH=${ZEST_BASE}/client/linux-mt ${ZEST_BASE}/scripts/zcc
endif

ifneq ($(filter numa,${MODULES}),)
  ifdef PICKLE_HAVE_NUMA
    LDFLAGS+=	${NUMA_LIBS}
  endif
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
  DEFINES+=	-DPFL_CTL
endif

ifneq ($(filter sgio,${MODULES}),)
  ifdef PICKLE_HAVE_SGIO
    SRCS+=	${ZEST_BASE}/sgio/sg.c
    SRCS+=	${ZEST_BASE}/sgio/sg_async.c
  endif
endif

ifneq ($(filter sqlite,${MODULES}),)
  CFLAGS+=	${SQLITE3_CFLAGS}
  LDFLAGS+=	${SQLITE3_LIBS}
  DEFINES+=	${SQLITE3_DEFINES}
  INCLUDES+=	${SQLITE3_INCLUDES}
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

ifneq ($(filter clock,${MODULES}),)
  SRCS+=	${CLOCK_SRCS}
  MODULES+=	rt
endif

ifneq ($(filter dl,${MODULES}),)
  LDFLAGS+=	${LIBDL}
endif

ifneq ($(filter rt,${MODULES}),)
  LDFLAGS+=	${LIBRT}
endif

ifeq (${LIBRARY},libpfl.a)
  ifneq ($(filter gcrc,${MODULES}),)
    INCLUDES+=	-I${GCRC_BASE}
    SRCS+=	${PFL_BASE}/gcrcutil.cc
    SRCS+=	${GCRC_BASE}/crc32c_sse4.cc
    SRCS+=	${GCRC_BASE}/interface.cc
    SRCS+=	${GCRC_BASE}/multiword_128_64_gcc_amd64_sse2.cc
    SRCS+=	${GCRC_BASE}/multiword_64_64_cl_i386_mmx.cc
    SRCS+=	${GCRC_BASE}/multiword_64_64_gcc_amd64_asm.cc
    SRCS+=	${GCRC_BASE}/multiword_64_64_gcc_i386_mmx.cc
    SRCS+=	${GCRC_BASE}/multiword_64_64_intrinsic_i386_mmx.cc
    CFLAGS+=	-mcrc32
  else
    SRCS+=	${PFL_BASE}/crc.c
  endif
else
  ifneq ($(filter gcrc,${MODULES}),)
    LDFLAGS+=	-lstdc++
  endif
endif

# OBJDIR is added to .c below since lex/yacc intermediate files get
# generated there.
vpath %.y  $(sort $(dir $(filter %.y,${_TSRCS})))
vpath %.l  $(sort $(dir $(filter %.l,${_TSRCS})))
vpath %.cc $(sort $(dir $(filter %.cc,${_TSRCS})))
vpath %.c  $(sort $(dir $(filter %.c,${_TSRCS})) ${OBJDIR})

_TDEPLIST=	$(addprefix dep-,$(subst :,@,${DEPLIST}))

all: checksrcs ${_TDEPLIST} recurse-all all-hook ${OBJDIR}
	@if ${NOTEMPTY} "${TARGET}"; then				\
		${MAKE} ${TARGET};					\
	fi

# XXX although these aren't real targets and should be marked PHONY,
# enabling this line causes nothing to actually build correctly.
#.PHONY: checksrcs ${_TDEPLIST} all recurse-all all-hook $(addprefix dir-,${SUBDIRS})

${OBJDIR}:
	@${MKDIRS} -m 2775 ${OBJDIR}

checksrcs: ${SRCS}
	@for i in ${SRCS}; do						\
		[ -n "$$i" ] || continue;				\
		if ! [ -e "$$i" ]; then					\
			echo "$$i does not exist" >&2;			\
			exit 1;						\
		fi;							\
	done

${_TDEPLIST}:
	@dep="$(patsubst dep-%,%,$@)";					\
	dir="$${dep%@*}";						\
	target="$${dep#*@}";						\
	${SYNCMAKE} "$$dir" "${MAKE}" -C "$$dir" $${target}

all-hook:

.SUFFIXES:

.PRECIOUS: ${OBJDIR}/$(notdir %.d)

# XXX this doesn't seem to work as advertised
.SILENT: ${OBJDIR}/$(notdir %.d)

${OBJDIR}/$(notdir %.o) : %.cc | ${OBJDIR}
	${PCPP} ${PCPP_FLAGS} $(call FILE_PCPP_FLAGS,$<) $(realpath $<	\
	    ) | ${CXX} -x c++ ${CFLAGS} $(call FILE_CFLAGS,$<) $(	\
	    ) $(filter-out ${_EXCLUDES},-I$(realpath $(dir $<))/) - -c -o $@ -MD -MP

${OBJDIR}/$(notdir %.o) : %.c | ${OBJDIR}
	${PCPP} ${PCPP_FLAGS} $(call FILE_PCPP_FLAGS,$<) $(realpath $<	\
	    ) | ${CC} -x c ${CFLAGS} $(call FILE_CFLAGS,$<) $(		\
	    ) $(filter-out ${_EXCLUDES},-I$(realpath $(dir $<))/) - -c -o $@ -MD -MP

${OBJDIR}/$(notdir %.E) : %.c | ${OBJDIR}
	${CC} ${CFLAGS} $(call FILE_CFLAGS,$<) $(realpath $<) $(	\
	    ) $(filter-out ${_EXCLUDES},-I$(realpath $(dir $<))/) -E -o $@

${OBJDIR}/$(notdir %.c) : %.l | ${OBJDIR}
	${LEX} ${LFLAGS} $(realpath $<) > $@

#	$(eval $$(call FILE_PCPP_FLAGS_VARNAME,$@)+=$$(call FILE_PCPP_FLAGS,$<))
#	$(eval $$(call FILE_CFLAGS_VARNAME,$@)+=$$(call FILE_CFLAGS,$<))

${OBJDIR}/$(notdir %.c) : %.y | ${OBJDIR}
	${YACC} ${YFLAGS} -o $@ $(realpath $<)

ifdef PROG
${_TPROG}: ${OBJS}
	${LD} -o $@ $(sort ${OBJS}) ${LDFLAGS}
	@printf "%s" "${_TPROG}:" > ${DEPEND_FILE}
	@${LIBDEP} ${LDFLAGS} ${LIBDEP_ADD} >> ${DEPEND_FILE}
endif

ifdef LIBRARY
  ${LIBRARY}: ${OBJS}
	${AR} ${ARFLAGS} $@ $(sort ${OBJS})
endif

ifdef SHLIB
  CFLAGS+=	-fPIC
  ${_TSHLIB}: ${OBJS}

	# Put libraries at the end to deal with --as-needed.
	 
	${CC} -shared -o $@ $(sort ${OBJS}) ${LDFLAGS} 
	@printf "%s" "${_TSHLIB}:" > ${DEPEND_FILE}
	@${LIBDEP} ${LDFLAGS} ${LIBDEP_ADD} >> ${DEPEND_FILE}
endif

%.dvi : ${OBJDIR}/$(notdir %.tex)
	${TEX} -output-directory=. $<

$(addprefix dir-,${SUBDIRS}):
	@${MAKE} -C $(patsubst dir-%,%,$@) ${MAKECMDGOALS}

recurse-%: $(addprefix dir-,${SUBDIRS})
	@:

# empty but overrideable
install-hook:
install-hook-post:

install: recurse-install install-hook
	@if [ -n "${LIBRARY}" -a x"${NOINSTALL}" != x"1" ]; then	\
		${INST} ${LIBRARY} ${INST_LIBDIR}/;			\
	fi
	@if [ x"${NOINSTALL}" != x"1" ]; then				\
		for shlib in ${SHLIB}; do				\
			src=$${shlib%:*};				\
			dst=$${shlib#*:};				\
			${INST} -m 555 $$src "${INST_LIBDIR}"/$$dst;	\
		done;							\
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
		for bin in ${PROG} ${BIN}; do				\
			srcbin=$${bin%:*};				\
			dstbin=$${bin#*:};				\
			[ -z "$$dstbin" ] && dstbin=$$srcbin;		\
			${INST} -m 555 $$srcbin "$$dir"/$$dstbin;	\
		done;							\
		# replace constants in installed scripts for system	\
		# settings						\
		for bin in ${BIN}; do					\
			if head -1 $$srcbin | grep -aq perl; then	\
				${ECHORUN} perl -i -Wpe 's{^# use $(	\
				    )lib qw\(%INST_PLMODDIR%\);$$}$(	\
				    ){use lib qw(${INST_PLMODDIR});}'	\
				    $$dir/$$dstbin;			\
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
	@${MAKE} -s install-hook-post

clean-hook:

distclean-hook:

clean-core:
	${RM} -rf ${OBJDIR}
	${RM} -f ${_TPROG} ${LIBRARY} ${_TSHLIB} core.[0-9]* *.core ${CLEANFILES}
	@for i in ${DEPLIST}; do					\
		[ -e "$${i#*:}" ] || continue;				\
		(cd $${i%:*} && ${MAKE} clean) || exit 1;		\
	done

clean: recurse-clean clean-core clean-hook

distclean: recurse-distclean clean-core clean-hook distclean-hook
	${RM} -f TAGS cscope.*out ${DISTCLEANFILES}

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

test:
	${MAKE} all && ${MAKE} runtest

hdrclean:
	${HDRCLEAN} */*.[clyh]

build:
	${MAKE} clean && ${MAKE} all

copyright:
	find . -type f \( $(						\
	    ) $(foreach ign,${COPYRIGHT_PATS},-name ${ign} -o) $(	\
	    ) -false \) -exec ${ECHORUN} ${ROOTDIR}/tools/gencopyright {} \;

doc: recurse-doc
	@if ${NOTEMPTY} "${MAN}"; then					\
		${ECHORUN} ${MDPROC} $$(echo ${MAN} $(			\
		    ) $$([ -e ${_TPROG}.[0-9] ] && echo ${_TPROG}.[0-9]) $(	\
		    ) $$([ -e ${LIBRARY}.[0-9] ] && $(			\
		    ) echo ${LIBRARY}.[0-9]) | tr ' ' '\n' | sort -u);	\
	fi

printvar-%:
	@echo ${$(patsubst printvar-%,%,$@)}

cscope cs: recurse-cs
	@if ${NOTEMPTY} "${_TSRCS}" || [ -n "${FORCE_TAGS}" ] ; then	\
		${ECHORUN} cscope -Rbq $(addprefix -s,$(filter-out	\
		    ${CURDIR},${_TSRC_PATH}));				\
	fi

etags et: recurse-etags
	find . ${_TSRC_PATH} -name \*.[chly] | xargs etags

printenv:
	@env | sort

scm-%:
	@${ROOTDIR}/tools/update -D '${RELCURDIR}' $@

up: scm-update

# LLVM hack with our pcpp
<stdin>:
	@touch '<stdin>'

-include ${DEPEND_FILE}
-include ${OBJDIR}/*.d
