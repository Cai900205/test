#!/bin/bash
# Copyright 2013        Mellanox Technologies. All rights reserved.
# Copyright 2012        Luis R. Rodriguez <mcgrof@frijolero.org>
# Copyright 2012        Hauke Mehrtens <hauke@hauke-m.de>
#
# This generates a bunch of CONFIG_COMPAT_KERNEL_2_6_22
# CONFIG_COMPAT_KERNEL_3_0 .. etc for each kernel release you need an object
# for.
#
# Note: this is part of the compat.git project, not compat-drivers,
# send patches against compat.git.

if [[ ! -f ${KLIB_BUILD}/Makefile ]]; then
	exit
fi

KERNEL_VERSION=$(${MAKE} -C ${KLIB_BUILD} kernelversion | sed -n 's/^\([0-9]\)\..*/\1/p')

# 3.0 kernel stuff
COMPAT_LATEST_VERSION="16"
KERNEL_SUBLEVEL="-1"

function set_config {
	VAR=$1
	VALUE=$2

	eval "export $VAR=$VALUE"
	echo "export $VAR=$VALUE"
}
function unset_config {
	VAR=$1

	eval "unset $VAR"
	echo "unexport $VAR"
}

function check_autofconf {
	VAR=$1
	VALUE=$(tac ${KLIB_BUILD}/include/*/autoconf.h | grep -m1 ${VAR} | sed -ne 's/.*\([01]\)$/\1/gp')

	eval "export $VAR=$VALUE"
}

function is_kernel_symbol_exported {
	SYMBOL=$1
	grep -wq ${SYMBOL} ${KLIB_BUILD}/*symvers* >/dev/null 2>&1
}

# Note that this script will export all variables explicitly,
# trying to export all with a blanket "export" statement at
# the top of the generated file causes the build to slow down
# by an order of magnitude.

if [[ ${KERNEL_VERSION} -eq "3" ]]; then
	KERNEL_SUBLEVEL=$(${MAKE} -C ${KLIB_BUILD} kernelversion | sed -n 's/^3\.\([0-9]\+\).*/\1/p')
else
	COMPAT_26LATEST_VERSION="39"
	KERNEL_26SUBLEVEL=$(${MAKE} -C ${KLIB_BUILD} kernelversion | sed -n 's/^2\.6\.\([0-9]\+\).*/\1/p')
	let KERNEL_26SUBLEVEL=${KERNEL_26SUBLEVEL}+1

	for i in $(seq ${KERNEL_26SUBLEVEL} ${COMPAT_26LATEST_VERSION}); do
		set_config CONFIG_COMPAT_KERNEL_2_6_${i} y
	done
fi

let KERNEL_SUBLEVEL=${KERNEL_SUBLEVEL}+1
for i in $(seq ${KERNEL_SUBLEVEL} ${COMPAT_LATEST_VERSION}); do
	set_config CONFIG_COMPAT_KERNEL_3_${i} y
done

# The purpose of these seem to be the inverse of the above other varibales.
# The RHEL checks seem to annotate the existance of RHEL minor versions.
RHEL_MAJOR=$(grep ^RHEL_MAJOR ${KLIB_BUILD}/Makefile | sed -n 's/.*= *\(.*\)/\1/p')
if [[ ! -z ${RHEL_MAJOR} ]]; then
	RHEL_MINOR=$(grep ^RHEL_MINOR ${KLIB_BUILD}/Makefile | sed -n 's/.*= *\(.*\)/\1/p')
	for i in $(seq 0 ${RHEL_MINOR}); do
		set_config CONFIG_COMPAT_RHEL_${RHEL_MAJOR}_${i} y
	done
fi

if [[ ${CONFIG_COMPAT_KERNEL_2_6_36} = "y" ]]; then
	if [[ ! ${CONFIG_COMPAT_RHEL_6_1} = "y" ]]; then
		set_config CONFIG_COMPAT_KFIFO y
	fi
fi

case ${KVERSION} in
	3.0.7[6-9]-[0-9].[0-9]* | 3.0.[8-9][0-9]-[0-9].[0-9]* | 3.0.[1-9][0-9][0-9]-[0-9].[0-9]*)
	SLES_11_3_KERNEL=${KVERSION}
	SLES_MAJOR="11"
	SLES_MINOR="3"
	set_config CONFIG_COMPAT_SLES_11_3 y
	;;
esac

SLES_11_2_KERNEL=$(echo ${KVERSION} | sed -n 's/^\(3\.0\.[0-9]\+\)\-\(.*\)\-\(.*\)/\1-\2-\3/p')
if [[ ! -z ${SLES_11_2_KERNEL} ]]; then
	SLES_MAJOR="11"
	SLES_MINOR="2"
	set_config CONFIG_COMPAT_SLES_11_2 y
fi

SLES_11_1_KERNEL=$(echo ${KVERSION} | sed -n 's/^\(2\.6\.32\.[0-9]\+\)\-\(.*\)\-\(.*\)/\1-\2-\3/p')
if [[ ! -z ${SLES_11_1_KERNEL} ]]; then
	SLES_MAJOR="11"
	SLES_MINOR="1"
	set_config CONFIG_COMPAT_SLES_11_1 y
fi

FC14_KERNEL=$(echo ${KVERSION} | grep fc14)
if [[ ! -z ${FC14_KERNEL} ]]; then
 # CONFIG_COMPAT_DISABLE_DCB should be set to 'y' as it used in drivers/net/ethernet/mellanox/mlx4/Makefile
	set_config CONFIG_COMPAT_DISABLE_DCB y
fi

FC16_KERNEL=$(echo ${KVERSION} | grep fc16)
if [[ ! -z ${FC16_KERNEL} ]]; then
	set_config CONFIG_COMPAT_EN_SYSFS y
fi

UBUNTU12_3_2=$(uname -v | grep -qs Ubuntu && echo ${KVERSION} | grep ^3\.2)
if [[ ! -z ${UBUNTU12_3_2} ]]; then
	set_config CONFIG_COMPAT_EN_SYSFS y
fi

if [ -e /etc/debian_version ]; then
	DEBIAN6=$(cat /etc/debian_version | grep 6\.0)
	if [[ ! -z ${DEBIAN6} ]]; then
		set_config CONFIG_COMPAT_DISABLE_DCB y
	fi
fi

if ! [[ $KVERSION =~ 2.6.16 ]]; then
	set_config CONFIG_COMPAT_SIGNATURE y
fi

if [[ ${CONFIG_COMPAT_KERNEL_2_6_38} = "y" ]]; then
	if [[ ! ${CONFIG_COMPAT_RHEL_6_3} = "y" ]]; then
		set_config CONFIG_COMPAT_NO_PRINTK_NEEDED y
	fi
fi

if [[ ${CONFIG_COMPAT_SLES_11_1} = "y" ]]; then
	set_config CONFIG_COMPAT_DISABLE_DCB y
	set_config CONFIG_COMPAT_UNDO_I6_PRINT_GIDS y
	set_config CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ y
fi

if [[ ${CONFIG_COMPAT_SLES_11_2} = "y" ]]; then
	set_config CONFIG_COMPAT_MIN_DUMP_ALLOC_ARG y
	set_config CONFIG_COMPAT_IS_NUM_TX_QUEUES y
	set_config CONFIG_COMPAT_NEW_TX_RING_SCHEME y
	set_config CONFIG_COMPAT_EN_SYSFS y
fi

if (grep -qw SRP_RPORT_LOST ${KLIB_BUILD}/include/scsi/scsi_transport_srp.h > /dev/null 2>&1 || grep -qw SRP_RPORT_LOST /lib/modules/${KVERSION}/source/include/scsi/scsi_transport_srp.h > /dev/null 2>&1); then
	set_config SRP_NO_FAST_IO_FAIL y
fi

if (grep -qw param_mask ${KLIB_BUILD}/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1 || grep -qw param_mask /lib/modules/${KVERSION}/source/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1) && \
	(grep -qw ISCSI_TGT_RESET_TMO ${KLIB_BUILD}/include/scsi/iscsi_if.h > /dev/null 2>&1 || grep -qw ISCSI_TGT_RESET_TMO /lib/modules/${KVERSION}/source/include/scsi/iscsi_if.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_ISCSI_TRANSPORT_PARAM_MASK y
fi

if (grep -qw __skb_tx_hash ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw __skb_tx_hash /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS___SKB_TX_HASH y
fi

if (grep -Eq "mode_t.*attr_is_visible" ${KLIB_BUILD}/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1 || grep -Eq "mode_t.*attr_is_visible" /lib/modules/${KVERSION}/source/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_ISER_ATTR_IS_VISIBLE y
fi

if (grep -qw get_ep_param ${KLIB_BUILD}/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1 || grep -qw get_ep_param /lib/modules/${KVERSION}/source/include/scsi/scsi_transport_iscsi.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_ISCSI_ISER_GET_EP_PARAM y
fi

if (grep -qw iscsi_scsi_req ${KLIB_BUILD}/include/scsi/iscsi_proto.h > /dev/null 2>&1 || grep -qw iscsi_scsi_req /lib/modules/${KVERSION}/source/include/scsi/iscsi_proto.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IF_ISCSI_SCSI_REQ y
fi

if (grep -q 'scsi_target_unblock(struct device \*, enum scsi_device_state)' ${KLIB_BUILD}/include/scsi/scsi_device.h  > /dev/null 2>&1 ||
    grep -q 'scsi_target_unblock(struct device \*, enum scsi_device_state)' /lib/modules/${KVERSION}/source/include/scsi/scsi_device.h  > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SCSI_TARGET_UNBLOCK y
fi

if [[ "${KVERSION}" == "2.6.32.43-0.4.1.xs1.6.10.784.170772xen" ]]; then
	set_config CONFIG_COMPAT_MIN_DUMP_ALLOC_ARG y
fi

if [[ "${KVERSION}" =~ "2.6.32.*xs.*xen" ]]; then
	set_config CONFIG_COMPAT_ALLOC_PAGES_ORDER_0 y
fi

if (grep -q dst_set_neighbour ${KLIB_BUILD}/include/net/dst.h > /dev/null 2>&1 || grep -q dst_set_neighbour /lib/modules/${KVERSION}/source/include/net/dst.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_DST_NEIGHBOUR y
fi

if (grep -q eth_hw_addr_random ${KLIB_BUILD}/include/linux/etherdevice.h > /dev/null 2>&1 || grep -q eth_hw_addr_random /lib/modules/${KVERSION}/source/include/linux/etherdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_ETH_HW_ADDR_RANDOM y
fi

if (grep -q lro_receive_frags ${KLIB_BUILD}/include/linux/inet_lro.h > /dev/null 2>&1 || grep -q lro_receive_frags /lib/modules/${KVERSION}/source/include/linux/inet_lro.h > /dev/null 2>&1); then
    check_autofconf CONFIG_INET_LRO
    if [[ X${CONFIG_INET_LRO} == "X1" ]]; then
        set_config CONFIG_COMPAT_LRO_ENABLED y
    fi
fi

if (grep -q dev_hw_addr_random ${KLIB_BUILD}/include/linux/etherdevice.h > /dev/null 2>&1 || grep -q dev_hw_addr_random /lib/modules/${KVERSION}/source/include/linux/etherdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_DEV_HW_ADDR_RANDOM y
fi

if (grep -qw "netdev_features_t" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "netdev_features_t" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETDEV_FEATURES y
fi

if (grep -qw "WORK_BUSY_PENDING" ${KLIB_BUILD}/include/linux/workqueue.h > /dev/null 2>&1 || grep -qw "WORK_BUSY_PENDING" /lib/modules/${KVERSION}/source/include/linux/workqueue.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_WORK_BUSY y
fi

if (grep -qw ieee_getmaxrate ${KLIB_BUILD}/include/net/dcbnl.h > /dev/null 2>&1 || grep -qw ieee_getmaxrate /lib/modules/${KVERSION}/source/include/net/dcbnl.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_MAXRATE y
fi

if (grep -qw ieee_getqcn ${KLIB_BUILD}/include/net/dcbnl.h > /dev/null 2>&1 || grep -qw ieee_getqcn /lib/modules/${KVERSION}/source/include/net/dcbnl.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_QCN y
fi

if (grep -qw reinit_completion ${KLIB_BUILD}/include/linux/completion.h > /dev/null 2>&1 || grep -qw reinit_completion /lib/modules/${KVERSION}/source/include/linux/completion.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_REINIT_COMPLETION y
fi

if (grep -qw "netdev_extended" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "netdev_extended" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_NETDEV_EXTENDED y
fi

if (grep -qw "net_device_ops_ext" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "net_device_ops_ext" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_NETDEV_OPS_EXTENDED y
fi

if !(grep -qw "struct va_format" ${KLIB_BUILD}/include/linux/printk.h > /dev/null 2>&1 || grep -qw "struct va_format" /lib/modules/${KVERSION}/source/include/linux/printk.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_DISABLE_VA_FORMAT_PRINT y
fi
if (grep -qw "netif_is_bond_master" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "netif_is_bond_master" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_IS_BOND_MASTER y
fi
if (grep -qw "struct xps_map" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "struct xps_map" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_IS_XPS y
fi
if (grep -qw "__netdev_pick_tx" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "__netdev_pick_tx" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_HAS_PICK_TX y
fi
if (grep -qw "netif_set_xps_queue" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "netif_set_xps_queue" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_HAS_SET_XPS_QUEUE y
fi
if (grep -qw "sk_tx_queue_get" ${KLIB_BUILD}/include/net/sock.h > /dev/null 2>&1 || grep -qw "sk_tx_queue_get" /lib/modules/${KVERSION}/source/include/net/sock.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SOCK_HAS_QUEUE y
fi
if (grep -qw "skb_has_frag_list" ${KLIB_BUILD}/include/linux/skbuff.h > /dev/null 2>&1 || grep -qw "skb_has_frag_list" /lib/modules/${KVERSION}/source/include/linux/skbuff.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SKB_HAS_FRAG_LIST y
fi
if (grep -qw "irq_set_affinity_hint" ${KLIB_BUILD}/include/linux/interrupt.h > /dev/null 2>&1 || grep -qw "irq_set_affinity_hint" /lib/modules/${KVERSION}/source/include/linux/interrupt.h > /dev/null 2>&1); then
        set_config CONFIG_COMPAT_HAS_IRQ_AFFINITY_HINT y
fi
if (grep -qw "pm_qos_add_requirement" ${KLIB_BUILD}/include/linux/pm_qos_params.h > /dev/null 2>&1 || grep -qw "pm_qos_add_requirement" /lib/modules/${KVERSION}/source/include/linux/pm_qos_params.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_PM_QOS y
fi
if (grep -qw "struct pm_qos_request_list" ${KLIB_BUILD}/include/linux/pm_qos_params.h > /dev/null 2>&1 || grep -qw "struct pm_qos_request_list" /lib/modules/${KVERSION}/source/include/linux/pm_qos_params.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_PM_QOS y
	set_config CONFIG_COMPAT_PM_QOS_V1 y
fi
if (grep -qw "struct pm_qos_request" ${KLIB_BUILD}/include/linux/pm_qos.h > /dev/null 2>&1 || grep -qw "struct pm_qos_request" /lib/modules/${KVERSION}/source/include/linux/pm_qos.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_PM_QOS y
	set_config CONFIG_COMPAT_PM_QOS_V2 y
fi
if (grep -qw "NETIF_F_RXHASH" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "NETIF_F_RXHASH" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_F_RXHASH y
fi
if (grep -qw "NETIF_F_RXHASH" ${KLIB_BUILD}/include/linux/netdev_features.h > /dev/null 2>&1 || grep -qw "NETIF_F_RXHASH" /lib/modules/${KVERSION}/source/include/linux/netdev_features.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NETIF_F_RXHASH y
fi
if (grep -q "struct cpu_rmap {" ${KLIB_BUILD}/include/linux/cpu_rmap.h > /dev/null 2>&1 || grep -q "struct cpu_rmap {" /lib/modules/${KVERSION}/source/include/linux/cpu_rmap.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_LINUX_CPU_RMAP y
fi

if [[ ${CONFIG_COMPAT_RHEL_6_4} = "y" ]]; then
	set_config CONFIG_COMPAT_NETLINK_3_7 y
	set_config CONFIG_COMPAT_HAS_NUM_CHANNELS y
	set_config CONFIG_COMPAT_ETHTOOL_OPS_EXT y
fi

if [[ ${RHEL_MAJOR} -eq "6" ]]; then
	set_config CONFIG_COMPAT_DEFINE_NUM_LRO y
	set_config CONFIG_COMPAT_EN_SYSFS y
	set_config CONFIG_COMPAT_LOOPBACK y

	if [[ ${RHEL_MINOR} -ne "1" ]]; then
		set_config CONFIG_COMPAT_IS_NUM_TX_QUEUES y
		set_config CONFIG_COMPAT_NEW_TX_RING_SCHEME y
	fi

	if [[ ${RHEL_MINOR} -eq "1" ]]; then
		set_config CONFIG_COMPAT_DISABLE_DCB y
	fi
fi

if (grep -qw kfree_rcu ${KLIB_BUILD}/include/linux/rcupdate.h > /dev/null 2>&1 || grep -qw kfree_rcu /lib/modules/${KVERSION}/source/include/linux/rcupdate.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_RCU y
fi

if (grep -q kstrto ${KLIB_BUILD}/include/linux/kernel.h > /dev/null 2>&1 || grep -q kstrto /lib/modules/${KVERSION}/source/include/linux/kernel.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_KSTRTOX y
fi

if (is_kernel_symbol_exported ip_tos2prio); then
	set_config CONFIG_COMPAT_IS_IP_TOS2PRIO y
fi

if (grep -qw test_bit_le ${KLIB_BUILD}/include/asm-generic/bitops/le.h > /dev/null 2>&1 || grep -qw test_bit_le /lib/modules/${KVERSION}/source/include/asm-generic/bitops/le.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_BITOP y
fi

if (grep -qw "dev_uc_add(struct net_device \*dev, const unsigned char \*addr)" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw "dev_uc_add(struct net_device \*dev, const unsigned char \*addr)" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_DEV_UC_MC_ADD_CONST y
fi

if (grep -qw ndo_set_vf_mac ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw ndo_set_vf_mac /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NDO_VF_MAC_VLAN y
fi

if (grep -qw ndo_set_vf_spoofchk ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw ndo_set_vf_spoofchk /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_VF_INFO_SPOOFCHK y
fi

if (grep -qw ndo_set_vf_link_state ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw ndo_set_vf_link_state /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_VF_INFO_LINKSTATE y
fi

if (grep -qw pci_physfn ${KLIB_BUILD}/include/linux/pci.h > /dev/null 2>&1 || grep -qw pci_physfn /lib/modules/${KVERSION}/source/include/linux/pci.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IS_PCI_PHYSFN y
fi

if (grep -q "xprt_reserve_xprt_cong.*rpc_xprt" ${KLIB_BUILD}/include/linux/sunrpc/xprt.h > /dev/null 2>&1 || grep -q "xprt_reserve_xprt_cong.*rpc_xprt" /lib/modules/${KVERSION}/source/include/linux/sunrpc/xprt.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_XPRT_RESERVE_XPRT_CONG_2PARAMS y
fi

if (grep -qw num_prealloc ${KLIB_BUILD}/include/linux/sunrpc/xprt.h > /dev/null 2>&1 || grep -qw num_prealloc /lib/modules/${KVERSION}/source/include/linux/sunrpc/xprt.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_XPRT_ALLOC_4PARAMS y
fi

if (grep -qw tk_bytes_sent ${KLIB_BUILD}/include/linux/sunrpc/sched.h > /dev/null 2>&1 || grep -qw tk_bytes_sent /lib/modules/${KVERSION}/source/include/linux/sunrpc/sched.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_XPRT_TK_BYTES_SENT y
fi

if (grep -qw rpc_xprt ${KLIB_BUILD}/include/linux/sunrpc/xprt.h > /dev/null 2>&1 || grep -qw rpc_xprt /lib/modules/${KVERSION}/source/include/linux/sunrpc/xprt.h > /dev/null 2>&1) && \
	(grep -qw rq_xmit_bytes_sent ${KLIB_BUILD}/include/linux/sunrpc/xprt.h > /dev/null 2>&1 || grep -qw rq_xmit_bytes_sent /lib/modules/${KVERSION}/source/include/linux/sunrpc/xprt.h > /dev/null 2>&1) && \
	(grep -qw xprt_alloc ${KLIB_BUILD}/include/linux/sunrpc/xprt.h > /dev/null 2>&1 || grep -qw xprt_alloc /lib/modules/${KVERSION}/source/include/linux/sunrpc/xprt.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_XPRTRDMA_NEEDED y
fi

if (grep -q virtqueue_get_buf ${KLIB_BUILD}/include/linux/virtio.h > /dev/null 2>&1 || grep -q virtqueue_get_buf /lib/modules/${KVERSION}/source/include/linux/virtio.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_VIRTQUEUE_GET_BUF y
fi

if (grep -q virtqueue_add_buf ${KLIB_BUILD}/include/linux/virtio.h > /dev/null 2>&1 || grep -q virtqueue_add_buf /lib/modules/${KVERSION}/source/include/linux/virtio.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_VIRTQUEUE_ADD_BUF y
fi

if (grep -q virtqueue_kick ${KLIB_BUILD}/include/linux/virtio.h > /dev/null 2>&1 || grep -q virtqueue_kick /lib/modules/${KVERSION}/source/include/linux/virtio.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_VIRTQUEUE_KICK y
fi

if (grep -q zc_request ${KLIB_BUILD}/include/net/9p/transport.h > /dev/null 2>&1 || grep -q zc_request /lib/modules/${KVERSION}/source/include/net/9p/transport.h > /dev/null 2>&1); then
        set_config CONFIG_COMPAT_ZC_REQUEST y
fi

if (grep -q gfp_t ${KLIB_BUILD}/tools/include/virtio/linux/virtio.h > /dev/null 2>&1 || grep -q gfp_t /lib/modules/${KVERSION}/source/include/linux/virtio.h > /dev/null 2>&1); then
        set_config CONFIG_COMPAT_GFP_T y
fi

if (grep -q virtqueue_add_buf_gfp ${KLIB_BUILD}/tools/include/virtio/linux/virtio.h > /dev/null 2>&1 || grep -q virtqueue_add_buf_gfp /lib/modules/${KVERSION}/source/include/linux/virtio.h > /dev/null 2>&1); then
        set_config CONFIG_COMPAT_VIRTQUEUE_ADD_BUF_GFP y
fi

if (grep -q TCA_CODEL_UNSPEC ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q TCA_CODEL_UNSPEC /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TCA_CODEL_UNSPEC y
fi

if (grep -q TCA_FQ_CODEL_UNSPEC ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q TCA_FQ_CODEL_UNSPEC /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TCA_FQ_CODEL_UNSPEC y
fi

if (grep -q "struct tc_codel_xstats {" ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q "struct tc_codel_xstats {" /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TC_CODEL_XSTATS y
fi

if (grep -q TCA_FQ_CODEL_XSTATS_QDISC ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q TCA_FQ_CODEL_XSTATS_QDISC /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TCA_FQ_CODEL_XSTATS_QDISC y
fi

if (grep -q "struct tc_fq_codel_xstats {" ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q "struct tc_fq_codel_xstats {" /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TC_FQ_CODEL_XSTATS y
fi

if (grep -q "struct tc_fq_codel_cl_stats {" ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q "struct tc_fq_codel_cl_stats {" /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TC_FQ_CODEL_CL_STATS y
fi

if (grep -q "struct tc_fq_codel_qd_stats {" ${KLIB_BUILD}/include/linux/pkt_sched.h > /dev/null 2>&1 || grep -q "struct tc_fq_codel_qd_stats {" /lib/modules/${KVERSION}/source/include/linux/pkt_sched.h > /dev/null 2>&1); then
        set_config CONFIG_TC_FQ_CODEL_QD_STATS y
fi

if (grep -q iscsi_eh_target_reset ${KLIB_BUILD}/include/scsi/libiscsi.h > /dev/null 2>&1 || grep -q iscsi_eh_target_reset /lib/modules/${KVERSION}/source/include/scsi/libiscsi.h > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_ISCSI_EH_TARGET_RESET y
fi

if (grep -q bitmap_set ${KLIB_BUILD}/include/linux/bitmap.h > /dev/null 2>&1 || grep -q bitmap_set /lib/modules/${KVERSION}/source/include/linux/bitmap.h > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_IS_BITMAP y
fi

if (grep -Eq "struct in_device.*idev" ${KLIB_BUILD}/include/net/route.h > /dev/null 2>&1 || grep -Eq "struct in_device.*idev" /lib/modules/${KVERSION}/source/include/net/route.h > /dev/null 2>&1); then
		set_config CONFIG_IS_RTABLE_IDEV y
fi

if (grep -q netdev_get_prio_tc_map ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || egrep -q netdev_get_prio_tc_map /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_IS_PRIO_TC_MAP y
fi

if (grep -q rx_handler_result ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || egrep -q rx_handler_result /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
		set_config CONFIG_IS_RX_HANDLER_RESULT y
fi

if (grep -q ndo_add_slave ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || egrep -q ndo_add_slave /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
		set_config CONFIG_IS_NDO_ADD_SLAVE y
fi

if (grep -q get_module_eeprom ${KLIB_BUILD}/include/linux/ethtool.h > /dev/null 2>&1 || grep -q get_module_eeprom /lib/modules/${KVERSION}/source/include/linux/ethtool.h > /dev/null 2>&1); then
         set_config CONFIG_MODULE_EEPROM_ETHTOOL y
fi

if (grep -q get_ts_info ${KLIB_BUILD}/include/linux/ethtool.h > /dev/null 2>&1 || grep -q get_ts_info /lib/modules/${KVERSION}/source/include/linux/ethtool.h > /dev/null 2>&1); then
        set_config CONFIG_TIMESTAMP_ETHTOOL y
fi

if (grep -q get_rxfh_indir ${KLIB_BUILD}/include/linux/ethtool.h > /dev/null 2>&1 || grep -q get_rxfh_indir /lib/modules/${KVERSION}/source/include/linux/ethtool.h > /dev/null 2>&1); then
        set_config CONFIG_COMPAT_INDIR_SETTING y
fi

if ! (grep -q get_channels ${KLIB_BUILD}/include/linux/ethtool.h > /dev/null 2>&1 || grep -q get_channels /lib/modules/${KVERSION}/source/include/linux/ethtool.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_NUM_CHANNELS y
fi

if ! (grep -q dcbnl_rtnl_ops ${KLIB_BUILD}/include/net/dcbnl.h > /dev/null 2>&1 || grep -q dcbnl_rtnl_ops /lib/modules/${KVERSION}/source/include/net/dcbnl.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_DISABLE_DCB y
fi

check_autofconf CONFIG_NET_SCH_MQPRIO_MODULE
if [[ X${CONFIG_NET_SCH_MQPRIO_MODULE} != "X1" ]]; then
	if  (grep -q netdev_get_prio_tc_map ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -q netdev_get_prio_tc_map /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
		if [[ ! ${CONFIG_COMPAT_RHEL_6_1} = "y" ]]; then
			set_config CONFIG_COMPAT_MQPRIO y
		fi
	else
		set_config CONFIG_COMPAT_DISABLE_DCB y
	fi
fi

if [ X${CONFIG_COMPAT_IS_MAXRATE} != "Xy" -o X${CONFIG_COMPAT_MQPRIO} = "Xy" -o X${CONFIG_COMPAT_INDIR_SETTING} = "Xy" -o X${CONFIG_COMPAT_NUM_CHANNELS} = "Xy" -o X${CONFIG_COMPAT_LOOPBACK} = "Xy" -o X${CONFIG_COMPAT_IS_QCN} != "Xy" ]; then
	set_config CONFIG_COMPAT_EN_SYSFS y
fi

if (grep -q skb_inner_transport_header ${KLIB_BUILD}/include/linux/skbuff.h > /dev/null 2>&1 || grep -q skb_inner_transport_header /lib/modules/${KVERSION}/source/include/linux/skbuff.h > /dev/null 2>&1); then
	check_autofconf CONFIG_VXLAN
	if [[ X${CONFIG_VXLAN} == "X1" ]]; then
		set_config CONFIG_COMPAT_VXLAN_ENABLED y
		if (grep ndo_add_vxlan_port ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep ndo_add_vxlan_port /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
			set_config CONFIG_COMPAT_VXLAN_DYNAMIC_PORT y
		fi
	fi
fi

if (grep -qw __IFLA_VF_LINK_STATE_MAX ${KLIB_BUILD}/include/uapi/linux/if_link.h > /dev/null 2>&1 || grep -qw __IFLA_VF_LINK_STATE_MAX /lib/modules/${KVERSION}/source/include/uapi/linux/if_link.h > /dev/null 2>&1 ||
    grep -qw __IFLA_VF_LINK_STATE_MAX ${KLIB_BUILD}/include/linux/if_link.h > /dev/null 2>&1 || grep -qw __IFLA_VF_LINK_STATE_MAX /lib/modules/${KVERSION}/source/include/linux/if_link.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_IFLA_VF_LINK_STATE_MAX y
fi

if (grep -qw vlan_dev_get_egress_qos_mask ${KLIB_BUILD}/include/linux/if_vlan.h > /dev/null 2>&1 || grep -qw vlan_dev_get_egress_qos_mask /lib/modules/${KVERSION}/source/include/linux/if_vlan.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_VLAN_EGRESS_VISIBLE y
fi

if ! (grep -qw "genl_register_family_with_ops_groups" ${KLIB_BUILD}/include/net/genetlink.h > /dev/null 2>&1 || grep -qw "genl_register_family_with_ops_groups" /lib/modules/${KVERSION}/source/include/net/genetlink.h > /dev/null 2>&1); then
	set_config CONFIG_GENETLINK_IS_LIST_HEAD y
fi

if (grep -q "unsigned lockless" ${KLIB_BUILD}/include/scsi/scsi_host.h > /dev/null 2>&1 || grep -q "unsigned lockless" /lib/modules/${KVERSION}/source/include/scsi/scsi_host.h > /dev/null 2>&1); then
	set_config CONFIG_IS_SCSI_LOCKLESS y
fi

if (grep -qw ISCSI_PARAM_DISCOVERY_SESS ${KLIB_BUILD}/include/scsi/iscsi_if.h > /dev/null 2>&1 || grep -qw ISCSI_PARAM_DISCOVERY_SESS /lib/modules/${KVERSION}/source/include/scsi/iscsi_if.h > /dev/null 2>&1); then
	if [[ ${CONFIG_COMPAT_RHEL_7_0} = "y" ]]; then
		set_config CONFIG_ISER_DISCOVERY y
	fi
fi

if (grep -q "int  (\*setnumtcs)(struct net_device \*, int, u8)" ${KLIB_BUILD}/include/net/dcbnl.h > /dev/null 2>&1 || grep -q "int  (\*setnumtcs)(struct net_device \*, int, u8)" /lib/modules/${KVERSION}/source/include/net/dcbnl.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SETNUMTCS_INT y
fi

if ((grep -q ndo_fdb_add ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -q ndo_fdb_add /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1) &&
    !(grep -q net_device_extended ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -q net_device_extended /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1)); then
	set_config CONFIG_COMPAT_FDB_API_EXISTS y

	if (grep ndo_fdb_add -A 4 ${KLIB_BUILD}/include/linux/netdevice.h 2>/dev/null | grep -q "struct nlattr \*tb" > /dev/null 2>&1 || grep ndo_fdb_add -A 4 /lib/modules/${KVERSION}/source/include/linux/netdevice.h 2>/dev/null | grep -q "struct nlattr \*tb" > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_FDB_ADD_NLATTR y
	fi

	if (grep ndo_fdb_del -A 3 ${KLIB_BUILD}/include/linux/netdevice.h 2>/dev/null | grep -q "struct nlattr \*tb" > /dev/null 2>&1 || grep ndo_fdb_del -A 3 /lib/modules/${KVERSION}/source/include/linux/netdevice.h 2>/dev/null | grep -q "struct nlattr \*tb" > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_FDB_DEL_NLATTR y
	fi

	if (grep ndo_fdb_add -A 4 ${KLIB_BUILD}/include/linux/netdevice.h 2>/dev/null | grep -q "const unsigned char \*addr" > /dev/null 2>&1 || grep ndo_fdb_add -A 4 /lib/modules/${KVERSION}/source/include/linux/netdevice.h 2>/dev/null | grep -q "const unsigned char \*addr" > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_FDB_CONST_ADDR y
	fi
fi

if (grep -q "const struct sysfs_ops \n*sysfs_ops" ${KLIB_BUILD}/include/linux/kobject.h > /dev/null 2>&1 || grep -q "const struct sysfs_ops \*sysfs_ops" /lib/modules/${KVERSION}/source/include/linux/kobjetc.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SYSFS_OPS_CONST y
fi

if (grep -q "void \*accel_priv" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -q "void \*accel_priv" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SELECT_QUEUE_ACCEL y
fi

if (grep -q "select_queue_fallback_t" ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -q "select_queue_fallback_t" /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_SELECT_QUEUE_FALLBACK y
fi

if (grep -q ptp_clock_info ${KLIB_BUILD}/include/linux/ptp_clock_kernel.h > /dev/null 2>&1 || grep -q ptp_clock_info /lib/modules/${KVERSION}/source/include/linux/ptp_clock_kernel.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_PTP_CLOCK y
	if (grep -q n_pins ${KLIB_BUILD}/include/linux/ptp_clock_kernel.h > /dev/null 2>&1 || grep -q n_pins /lib/modules/${KVERSION}/source/include/linux/ptp_clock_kernel.h > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_PTP_N_PINS y
	fi

	if (grep ptp_clock_register -A 1 ${KLIB_BUILD}/include/linux/ptp_clock_kernel.h 2>/dev/null | grep -q "struct device \*parent"> /dev/null 2>&1 || grep ptp_clock_register -A 1 /lib/modules/${KVERSION}/source/include/linux/ptp_clock_kernel.h 2>/dev/null | grep -q "struct device \*parent" > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_PTP_CLOCK_REGISTER y
	fi
fi

if (grep -q THIS_MODULE ${KLIB_BUILD}/include/linux/export.h > /dev/null 2>&1 || grep -q THIS_MODULE /lib/modules/${KVERSION}/source/include/linux/export.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_THIS_MODULE y
fi

if (grep -q "struct timecompare" ${KLIB_BUILD}/include/linux/timecompare.h > /dev/null 2>&1 || grep -q "struct timecompare" /lib/modules/${KVERSION}/source/include/linux/timecompare.h > /dev/null 2>&1); then
	if !(grep -q ptp_clock_info ${KLIB_BUILD}/include/linux/ptp_clock_kernel.h > /dev/null 2>&1 || grep -q ptp_clock_info /lib/modules/${KVERSION}/source/include/linux/ptp_clock_kernel.h > /dev/null 2>&1); then
		set_config CONFIG_COMPAT_TIMECOMPARE y
	fi
fi

if ! is_kernel_symbol_exported __put_task_struct || ! is_kernel_symbol_exported get_pid_task || ! is_kernel_symbol_exported get_task_pid; then
	set_config CONFIG_COMPAT_MISS_TASK_FUNCS y
fi

if (grep -q 'const void \*(\*namespace)(struct class \*class' ${KLIB_BUILD}/include/linux/device.h > /dev/null 2>&1 || grep -q 'const void \*(\*namespace)(struct class \*class' /lib/modules/${KVERSION}/source/include/linux/device.h > /dev/null 2>&1); then
	set_config CONFIG_COMPAT_CLASS_ATTR_NAMESPACE y
fi

if (grep -q "spinlock_t\s*frwd_lock;" ${KLIB_BUILD}/include/scsi/libiscsi.h > /dev/null 2>&1 || grep -q "spinlock_t\s*frwd_lock;" /lib/modules/${KVERSION}/source/include/scsi/libiscsi.h  > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_ISCSI_SESSION_FRWD_LOCK y
fi

if (grep -qw "compound_trans_head" ${KLIB_BUILD}/include/linux/huge_mm.h > /dev/null 2>&1 || grep -qw "compound_trans_head" /lib/modules/${KVERSION}/source/include/linux/huge_mm.h  > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_USE_COMPOUND_TRANS_HEAD y
fi

if (grep -qw "max_tx_rate" ${KLIB_BUILD}/include/linux/if_link.h > /dev/null 2>&1 || grep -qw "max_tx_rate" /lib/modules/${KVERSION}/source/include/linux/if_link.h  > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_IS_VF_INFO_MAX_TX_RATE y
fi

#‘struct net_device’ has no member named ‘num_tc’
if (grep -qw 'dev->num_tc' ${KLIB_BUILD}/include/linux/netdevice.h > /dev/null 2>&1 || grep -qw 'dev->num_tc' /lib/modules/${KVERSION}/source/include/linux/netdevice.h > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_NET_DEVICE_IS_NUM_TC y
fi

if (grep -Ewq "sysfs_dirent.*sysfs_get_dirent" ${KLIB_BUILD}/include/linux/sysfs.h > /dev/null 2>&1 || grep -Ewq "sysfs_dirent.*sysfs_get_dirent" /lib/modules/${KVERSION}/source/include/linux/sysfs.h > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_IS_SYSFS_DIRENT_SYSFS_GET_DIRENTY y
fi

if (grep -wq "ETH_FLAG_TXVLAN" ${KLIB_BUILD}/include/linux/ethtool.h > /dev/null 2>&1 || grep -wq "ETH_FLAG_TXVLAN" /lib/modules/${KVERSION}/source/include/linux/ethtool.h > /dev/null 2>&1); then
    set_config CONFIG_COMPAT_IS_ETH_FLAG_TXVLAN y
fi