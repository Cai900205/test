PHONY += all kernel install_kernel install clean clean_kernel
	
all:
.PHONY: $(PHONY)

.DELETE_ON_ERROR:

include ./configure.mk.kernel

export COMPAT_CONFIG=$(CWD)/compat.config
export COMPAT_AUTOCONF=$(CWD)/include/linux/compat_autoconf.h
export KLIB_BUILD
export KVERSION
export MAKE

-include $(COMPAT_CONFIG)

export CREL=$(shell cat $(CWD)/compat_version)
export CREL_PRE:=.compat_autoconf_
export CREL_CHECK:=$(CREL_PRE)$(CREL)
CFLAGS += \
	-DCOMPAT_BASE="\"$(shell cat compat_base)\"" \
	-DCOMPAT_BASE_TREE="\"$(shell cat compat_base_tree)\"" \
	-DCOMPAT_BASE_TREE_VERSION="\"$(shell cat compat_base_tree_version)\"" \
	-DCOMPAT_PROJECT="\"Compat-mlnx-ofed\"" \
	-DCOMPAT_VERSION="\"$(shell cat compat_version)\"" \

CC  = $(CROSS_COMPILE)gcc
CPP = $(CC) -E

# try-run
# Usage: option = $(call try-run, $(CC)...-o "$$TMP",option-ok,otherwise)
# Exit code chooses option. "$$TMP" is can be used as temporary file and
# is automatically cleaned up.
TMPOUT := /tmp/
try-run = $(shell set -e;				\
	TMP="$(TMPOUT).$$$$.tmp";		\
	TMPO="$(TMPOUT).$$$$.o";		\
	if ($(1)) >/dev/null 2>&1;		\
	then echo "$(2)";				\
	else echo "$(3)";				\
	fi;								\
	rm -f "$$TMP" "$$TMPO")

# cc-option
# Usage: cflags-y += $(call cc-option,-march=winchip-c6,-march=i586)
cc-option = $(call try-run,\
	$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_CFLAGS) $(1) -c -x c /dev/null -o "$$TMP",$(1),$(2))

ifneq ($(call cc-option,-Wno-date-time),)
KCFLAGS += -Wno-date-time
export KCFLAGS
endif

CPP_MAJOR := $(shell $(CPP) -dumpversion 2>&1 | cut -d'.' -f1)
CPP_MINOR := $(shell $(CPP) -dumpversion 2>&1 | cut -d'.' -f2)
CPP_PATCH := $(shell $(CPP) -dumpversion 2>&1 | cut -d'.' -f3)
# Assumes that major, minor, and patch cannot exceed 999
CPP_VERS  := $(shell expr 0$(CPP_MAJOR) \* 1000000 + 0$(CPP_MINOR) \* 1000 + 0$(CPP_PATCH))

ifneq (,$(filter $(ARCH), x86_64))
MLNX_CFLAGS += $(shell if [ $(CPP_VERS) -ge 4004004 ]; then \
                   echo "-Werror"; else echo ""; fi)
endif #(,$(filter $(ARCH), i386 x86_64))
export MLNX_CFLAGS

# GCC earlier than 4.6.0 will build modules which require 'mcount',
# and this symbol will not be available in the kernel if the kernel was
# compiled with GCC 4.6.0 and above.
# therefore, to prevent unknown symbol issues we disable function tracing.
#
compile_h=$(shell /bin/ls -1 $(KSRC)/include/*/compile.h 2> /dev/null | head -1)
ifneq ($(compile_h),)
KERNEL_GCC_MAJOR := $(shell grep LINUX_COMPILER $(compile_h) | sed -r -e 's/.*gcc version ([0-9\.\-]*) .*/\1/g' | cut -d'.' -f1)
KERNEL_GCC_MINOR := $(shell grep LINUX_COMPILER $(compile_h) | sed -r -e 's/.*gcc version ([0-9\.\-]*) .*/\1/g' | cut -d'.' -f2)
KERNEL_GCC_PATCH := $(shell grep LINUX_COMPILER $(compile_h) | sed -r -e 's/.*gcc version ([0-9\.\-]*) .*/\1/g' | cut -d'.' -f3)
KERNEL_GCC_VER  := $(shell expr 0$(KERNEL_GCC_MAJOR) \* 1000000 + 0$(KERNEL_GCC_MINOR) \* 1000 + 0$(KERNEL_GCC_PATCH))
ifneq ($(shell if [ $(CPP_VERS) -lt 4006000 ] && [ $(KERNEL_GCC_VER) -ge 4006000 ]; then \
					echo "YES"; else echo ""; fi),)
$(info Warning: The kernel was compiled with GCC newer than 4.6.0, while the current GCC is older than 4.6.0, Disabling function tracing to prevent unknown symbol issues...)
override WITH_MAKE_PARAMS += CONFIG_FUNCTION_TRACER= CONFIG_HAVE_FENTRY=
endif
endif

DEPMOD  = /sbin/depmod
INSTALL_MOD_DIR ?= $(shell test -f /etc/SuSE-release && echo updates || echo extra/mlnx-ofa_kernel)

ifeq ($(CONFIG_MEMTRACK),m)
        export KERNEL_MEMTRACK_CFLAGS = -include $(CWD)/drivers/infiniband/debug/mtrack.h
else
        export KERNEL_MEMTRACK_CFLAGS =
endif

ifeq ($(CONFIG_NFS_FS),m)
	export KERNEL_NFS_FS_CFLAGS = -I$(CWD)/include-nfs
else
	export KERNEL_NFS_FS_CFLAGS = 
endif

export OPEN_ISCSI_MODULES = iscsi_tcp.ko libiscsi.ko scsi_transport_iscsi.ko

$(COMPAT_AUTOCONF): $(COMPAT_CONFIG)
	+@$(CWD)/ofed_scripts/gen-compat-autoconf.sh $(COMPAT_CONFIG) > $(COMPAT_AUTOCONF)

$(COMPAT_CONFIG):
	+@$(CWD)/ofed_scripts/gen-compat-config.sh > $(COMPAT_CONFIG)

configure.mk.kernel:
	@echo Please run ./configure
	@exit 1

all: kernel

install: install_kernel
install_kernel: install_modules

autoconf_h=$(shell /bin/ls -1 $(KSRC)/include/*/autoconf.h 2> /dev/null | head -1)
kconfig_h=$(shell /bin/ls -1 $(KSRC)/include/*/kconfig.h 2> /dev/null | head -1)

ifneq ($(kconfig_h),)
KCONFIG_H = -include $(kconfig_h)
endif

V ?= 0

#########################
#	make kernel	#
#########################
#NB: The LINUXINCLUDE value comes from main kernel Makefile
#    with local directories prepended. This eventually affects
#    CPPFLAGS in the kernel Makefile
kernel: $(COMPAT_CONFIG) $(COMPAT_AUTOCONF)
	@echo "Building kernel modules"
	@echo "Kernel version: $(KVERSION)"
	@echo "Modules directory: $(INSTALL_MOD_PATH)/lib/modules/$(KVERSION)/$(INSTALL_MOD_DIR)"
	@echo "Kernel sources: $(KSRC)"
	env CWD=$(CWD) BACKPORT_INCLUDES=$(BACKPORT_INCLUDES) \
		$(MAKE) -C $(KSRC) SUBDIRS="$(CWD)" \
		V=$(V) KBUILD_NOCMDDEP=1 $(WITH_MAKE_PARAMS) \
		CONFIG_COMPAT_VERSION=$(CONFIG_COMPAT_VERSION) \
		CONFIG_COMPAT_KOBJECT_BACKPORT=$(CONFIG_COMPAT_KOBJECT_BACKPORT) \
		CONFIG_MEMTRACK=$(CONFIG_MEMTRACK) \
		CONFIG_DEBUG_INFO=$(CONFIG_DEBUG_INFO) \
		CONFIG_INFINIBAND=$(CONFIG_INFINIBAND) \
		CONFIG_INFINIBAND_IPOIB=$(CONFIG_INFINIBAND_IPOIB) \
		CONFIG_IPOIB_VERSION=$(CONFIG_IPOIB_VERSION) \
		CONFIG_INFINIBAND_IPOIB_CM=$(CONFIG_INFINIBAND_IPOIB_CM) \
		CONFIG_IPOIB_ALL_MULTI=$(CONFIG_IPOIB_ALL_MULTI) \
                CONFIG_INFINIBAND_MLNX_BX=$(CONFIG_INFINIBAND_MLNX_BX) \
		CONFIG_INFINIBAND_SDP=$(CONFIG_INFINIBAND_SDP) \
		CONFIG_INFINIBAND_SRP=$(CONFIG_INFINIBAND_SRP) \
		CONFIG_INFINIBAND_SRP_DUMMY=$(CONFIG_INFINIBAND_SRP_DUMMY) \
		CONFIG_SCSI_SRP_ATTRS=$(CONFIG_SCSI_SRP_ATTRS) \
		CONFIG_INFINIBAND_USER_MAD=$(CONFIG_INFINIBAND_USER_MAD) \
		CONFIG_INFINIBAND_USER_ACCESS=$(CONFIG_INFINIBAND_USER_ACCESS) \
		CONFIG_INFINIBAND_USER_MEM=$(CONFIG_INFINIBAND_USER_MEM) \
		CONFIG_INFINIBAND_ADDR_TRANS=$(CONFIG_INFINIBAND_ADDR_TRANS) \
		CONFIG_INFINIBAND_MTHCA=$(CONFIG_INFINIBAND_MTHCA) \
		CONFIG_INFINIBAND_IPOIB_DEBUG=$(CONFIG_INFINIBAND_IPOIB_DEBUG) \
		CONFIG_INFINIBAND_ISER=$(CONFIG_INFINIBAND_ISER) \
		CONFIG_SCSI_ISCSI_ATTRS=$(CONFIG_SCSI_ISCSI_ATTRS) \
		CONFIG_ISCSI_TCP=$(CONFIG_ISCSI_TCP) \
		CONFIG_INFINIBAND_EHCA=$(CONFIG_INFINIBAND_EHCA) \
		CONFIG_INFINIBAND_EHCA_SCALING=$(CONFIG_INFINIBAND_EHCA_SCALING) \
		CONFIG_E_IPOIB=$(CONFIG_E_IPOIB) \
		CONFIG_RDS=$(CONFIG_RDS) \
		CONFIG_RDS_RDMA=$(CONFIG_RDS_RDMA) \
		CONFIG_RDS_TCP=$(CONFIG_RDS_TCP) \
		CONFIG_RDS_DEBUG=$(CONFIG_RDS_DEBUG) \
		CONFIG_INFINIBAND_IPOIB_DEBUG_DATA=$(CONFIG_INFINIBAND_IPOIB_DEBUG_DATA) \
		CONFIG_INFINIBAND_SDP_SEND_ZCOPY=$(CONFIG_INFINIBAND_SDP_SEND_ZCOPY) \
		CONFIG_INFINIBAND_SDP_RECV_ZCOPY=$(CONFIG_INFINIBAND_SDP_RECV_ZCOPY) \
		CONFIG_INFINIBAND_SDP_DEBUG=$(CONFIG_INFINIBAND_SDP_DEBUG) \
		CONFIG_INFINIBAND_SDP_DEBUG_DATA=$(CONFIG_INFINIBAND_SDP_DEBUG_DATA) \
		CONFIG_INFINIBAND_IPATH=$(CONFIG_INFINIBAND_IPATH) \
		CONFIG_NET_9P_RDMA=$(CONFIG_NET_9P_RDMA) \
		CONFIG_9P_FS=$(CONFIG_9P_FS) \
		CONFIG_INFINIBAND_QIB=$(CONFIG_INFINIBAND_QIB) \
		CONFIG_INFINIBAND_MTHCA_DEBUG=$(CONFIG_INFINIBAND_MTHCA_DEBUG) \
		CONFIG_INFINIBAND_MADEYE=$(CONFIG_INFINIBAND_MADEYE) \
		CONFIG_INFINIBAND_QLGC_VNIC=$(CONFIG_INFINIBAND_QLGC_VNIC) \
		CONFIG_INFINIBAND_QLGC_VNIC_DEBUG=$(CONFIG_INFINIBAND_QLGC_VNIC_DEBUG) \
		CONFIG_INFINIBAND_QLGC_VNIC_STATS=$(CONFIG_INFINIBAND_QLGC_VNIC_STATS) \
		CONFIG_CHELSIO_T3=$(CONFIG_CHELSIO_T3) \
		CONFIG_INFINIBAND_CXGB3=$(CONFIG_INFINIBAND_CXGB3) \
		CONFIG_INFINIBAND_CXGB3_DEBUG=$(CONFIG_INFINIBAND_CXGB3_DEBUG) \
		CONFIG_CHELSIO_T4=$(CONFIG_CHELSIO_T4) \
		CONFIG_INFINIBAND_CXGB4=$(CONFIG_INFINIBAND_CXGB4) \
		CONFIG_SCSI_CXGB3_ISCSI=$(CONFIG_SCSI_CXGB3_ISCSI) \
		CONFIG_SCSI_CXGB4_ISCSI=$(CONFIG_SCSI_CXGB4_ISCSI) \
		CONFIG_INFINIBAND_NES=$(CONFIG_INFINIBAND_NES) \
		CONFIG_INFINIBAND_NES_DEBUG=$(CONFIG_INFINIBAND_NES_DEBUG) \
		CONFIG_MLX4_CORE=$(CONFIG_MLX4_CORE) \
		CONFIG_MLX5_CORE=$(CONFIG_MLX5_CORE) \
		CONFIG_MLX4_EN=$(CONFIG_MLX4_EN) \
		CONFIG_MLX4_EN_DCB=$(CONFIG_MLX4_EN_DCB) \
		CONFIG_MLX4_IB_DEBUG_FS=$(CONFIG_MLX4_IB_DEBUG_FS) \
		CONFIG_MLX4_VNIC=$(CONFIG_MLX4_VNIC) \
		CONFIG_MLX4_VNIC_DEBUG=$(CONFIG_MLX4_VNIC_DEBUG) \
		CONFIG_MLX4_INFINIBAND=$(CONFIG_MLX4_INFINIBAND) \
		CONFIG_MLX5_INFINIBAND=$(CONFIG_MLX5_INFINIBAND) \
		CONFIG_MLX4_ETHERNET=$(CONFIG_MLX4_ETHERNET) \
		CONFIG_MLX4_DEBUG=$(CONFIG_MLX4_DEBUG) \
		CONFIG_MLX5_DEBUG=$(CONFIG_MLX5_DEBUG) \
		CONFIG_MLX4_FC=$(CONFIG_MLX4_FC) \
                CONFIG_LIBFC=$(CONFIG_LIBFC) \
                CONFIG_LIBFCOE=$(CONFIG_LIBFCOE) \
		CONFIG_INFINIBAND_AMSO1100=$(CONFIG_INFINIBAND_AMSO1100) \
		CONFIG_SUNRPC_XPRT_RDMA=$(CONFIG_SUNRPC_XPRT_RDMA) \
		CONFIG_SUNRPC=$(CONFIG_SUNRPC) \
		CONFIG_SUNRPC_GSS=$(CONFIG_SUNRPC_GSS) \
		CONFIG_RPCSEC_GSS_KRB5=$(CONFIG_RPCSEC_GSS_KRB5) \
		CONFIG_RPCSEC_GSS_SPKM3=$(CONFIG_RPCSEC_GSS_SPKM3) \
		CONFIG_NFS_FS=$(CONFIG_NFS_FS) \
		CONFIG_NFS_V3=$(CONFIG_NFS_V3) \
		CONFIG_NFS_V3_ACL=$(CONFIG_NFS_V3_ACL) \
		CONFIG_NFS_V4=$(CONFIG_NFS_V4) \
		CONFIG_NFS_ACL_SUPPORT=$(CONFIG_NFS_ACL_SUPPORT) \
		CONFIG_NFS_DIRECTIO=$(CONFIG_NFS_DIRECTIO) \
		CONFIG_EXPORTFS=$(CONFIG_EXPORTFS)\
		CONFIG_LOCKD=$(CONFIG_LOCKD) \
		CONFIG_LOCKD_V4=$(CONFIG_LOCKD_V4) \
		CONFIG_NFSD=$(CONFIG_NFSD) \
		CONFIG_NFSD_V2_ACL=$(CONFIG_NFSD_V2_ACL) \
		CONFIG_NFSD_V3=$(CONFIG_NFSD_V3) \
		CONFIG_NFSD_V3_ACL=$(CONFIG_NFSD_V3_ACL) \
		CONFIG_NFSD_V4=$(CONFIG_NFSD_V4) \
		CONFIG_NFSD_RDMA=$(CONFIG_NFSD_RDMA) \
		CONFIG_INFINIBAND_OCRDMA=$(CONFIG_INFINIBAND_OCRDMA) \
		CONFIG_DTRACE= \
		CONFIG_INFINIBAND_ON_DEMAND_PAGING=$(CONFIG_INFINIBAND_ON_DEMAND_PAGING) \
		LINUXINCLUDE=' \
		-D__OFED_BUILD__ \
		$(CFLAGS) \
		-include $(autoconf_h) \
		$(KCONFIG_H) \
		-include $(CWD)/include/linux/compat-2.6.h \
		$(BACKPORT_INCLUDES) \
		$(KERNEL_MEMTRACK_CFLAGS) \
		$(KERNEL_NFS_FS_CFLAGS) \
		$(OPENIB_KERNEL_EXTRA_CFLAGS) \
		-I$(CWD)/include \
		-I$(CWD)/drivers/infiniband/debug \
		$$(if $$(CONFIG_XEN),-D__XEN_INTERFACE_VERSION__=$$(CONFIG_XEN_INTERFACE_VERSION)) \
		$$(if $$(CONFIG_XEN),-I$$(srctree)/arch/x86/include/mach-xen) \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		-Iinclude \
		-I$$(srctree)/arch/$$(SRCARCH)/include/uapi \
		-Iarch/$$(SRCARCH)/include/generated/uapi \
		-I$$(srctree)/include \
		-I$$(srctree)/include/uapi \
		-Iinclude/generated/uapi \
		$$(if $$(KBUILD_SRC),-Iinclude2 -I$$(srctree)/include) \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		' \
		modules


#########################
#	Install kernel	#
#########################
install_modules:
	@echo "Installing kernel modules"

ifeq ($(shell test -d /lib/modules/$(KVERSION)/updates/dkms && echo YES || echo ''),)
	$(MAKE) -C $(KSRC) SUBDIRS="$(CWD)" \
		INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
		INSTALL_MOD_DIR=$(INSTALL_MOD_DIR) \
		$(WITH_MAKE_PARAMS) modules_install;
else
	find $(CWD) -name "*.ko" -exec /bin/cp -f '{}' /lib/modules/$(KVERSION)/updates/dkms/ \;
endif

	if [ ! -n "$(INSTALL_MOD_PATH)" ]; then $(DEPMOD) $(KVERSION);fi;

clean: clean_kernel

clean_kernel:
	$(MAKE) -C $(KSRC) rm-files= SUBDIRS="$(CWD)" $(WITH_MAKE_PARAMS) clean

distclean: clean_kernel
	@/bin/rm -f $(clean-files)

clean-files := Module.symvers modules.order Module.markers compat/modules.order
clean-files += $(COMPAT_CONFIG) $(COMPAT_AUTOCONF)

help:
	@echo
	@echo kernel: 		        build kernel modules
	@echo all: 		        build kernel modules
	@echo
	@echo install_kernel:	        install kernel modules under $(INSTALL_MOD_PATH)/lib/modules/$(KVERSION)/$(INSTALL_MOD_DIR)
	@echo install:	        	run install_kernel
	@echo
	@echo clean:	        	delete kernel modules binaries
	@echo clean_kernel:	        delete kernel modules binaries
	@echo
