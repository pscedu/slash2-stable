# $Id$

MKDEP_PROG=	CC="${CC}" ${ROOTDIR}/tools/unwrapcc ${ROOTDIR}/tools/mkdep

# Disappointingly, recent versions of gcc hide
# standard headers in places other than /usr/include.
MKDEP=		${MKDEP_PROG} $$(if ${CC} -v 2>&1 | grep -q gcc; then \
		    ${CC} -print-search-dirs | grep install | \
		    awk '{print "-I" $$2 "include"}' | sed 's/:/ -I/'; fi)
LINT=		splint +posixlib
NOTEMPTY=	${ROOTDIR}/tools/notempty
PKG_CONFIG=	PKG_CONFIG_PATH=/usr/local/lib/pkgconfig pkg-config
LKERNEL_BASE=	${ROOTDIR}/kernel/2.6.9-42.0.8.EL_lustre.1.4.9.1


ifeq ($(wildcard /usr/src/kernels/linux),)
 ifeq ($(wildcard /opt/xt-os),)
  ifeq ($(wildcard /wolverine_pool),)
    # for ZESTIONs
    KERNEL_BASE=	/usr/src/kernels/2.6.18-92.el5-x86_64
  else
    # for wolverine
    KERNEL_BASE=	/usr/src/kernels/2.6.18-92.el5-xen-x86_64
  endif
 else
  ifeq ($(wildcard /opt/sgi),)
   # on xt3
   KERNEL_BASE=		/usr/src/kernel.2.6-ss-lustre26
  else
   # on altix
   KERNEL_BASE=		/usr/src/linux
  endif
 endif
else
 KERNEL_BASE=		/usr/src/kernels/linux
endif

ifneq ($(wildcard /opt/sgi),)
 # on altix
 DEFINES+=		-DCONFIG_NR_CPUS=2 -D_GNU_SOURCE -DHAVE_NUMA
 NUMA_LIBS=		-lcpuset -lbitmask -lnuma
endif

ifneq ($(wildcard /opt/xt-pe),)
 # on xt3
 SRCS+=			${PFL_BASE}/compat/posix_memalign.c
 DEFINES+=		-DHOST_NAME_MAX=MAXHOSTNAMELEN
endif

psc_fsutil_libs_psc_util_crc_c_CFLAGS=		-O2
psc_fsutil_libs_psc_util_parity_c_CFLAGS=	-O2

lnet_lite_socklnd_connection_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_pqtimer_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_procapi_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_proclib_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_select_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_table_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_tcplnd_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_socklnd_sendrecv_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET

lnet_lite_libcfs_debug_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_libcfs_nidstrings_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_libcfs_user_lock_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_libcfs_user_prim_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET

lnet_lite_lnet_acceptor_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_api_errno_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_api_ni_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_config_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lib_eq_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lib_md_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lib_me_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lib_move_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lib_msg_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_lo_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_peer_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_router_c_CFLAGS=		-DPSC_SUBSYS=PSS_LNET
lnet_lite_lnet_router_proc_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET

lnet_lite_ptllnd_ptllnd_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
lnet_lite_ptllnd_ptllnd_cb_c_CFLAGS=	-DPSC_SUBSYS=PSS_LNET
