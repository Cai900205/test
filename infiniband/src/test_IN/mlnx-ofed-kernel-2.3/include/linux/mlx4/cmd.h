/*
 * Copyright (c) 2006 Cisco Systems, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX4_CMD_H
#define MLX4_CMD_H

#include <linux/dma-mapping.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0))
#include <linux/if_link.h>
#endif

enum {
	/* initialization and general commands */
	MLX4_CMD_SYS_EN		 = 0x1,
	MLX4_CMD_SYS_DIS	 = 0x2,
	MLX4_CMD_MAP_FA		 = 0xfff,
	MLX4_CMD_UNMAP_FA	 = 0xffe,
	MLX4_CMD_RUN_FW		 = 0xff6,
	MLX4_CMD_MOD_STAT_CFG	 = 0x34,
	MLX4_CMD_QUERY_DEV_CAP	 = 0x3,
	MLX4_CMD_CONFIG_DEV	 = 0x3a,
	MLX4_CMD_QUERY_FW	 = 0x4,
	MLX4_CMD_ENABLE_LAM	 = 0xff8,
	MLX4_CMD_DISABLE_LAM	 = 0xff7,
	MLX4_CMD_QUERY_DDR	 = 0x5,
	MLX4_CMD_QUERY_ADAPTER	 = 0x6,
	MLX4_CMD_INIT_HCA	 = 0x7,
	MLX4_CMD_CLOSE_HCA	 = 0x8,
	MLX4_CMD_INIT_PORT	 = 0x9,
	MLX4_CMD_CLOSE_PORT	 = 0xa,
	MLX4_CMD_QUERY_HCA	 = 0xb,
	MLX4_CMD_QUERY_PORT	 = 0x43,
	MLX4_CMD_SENSE_PORT	 = 0x4d,
	MLX4_CMD_HW_HEALTH_CHECK = 0x50,
	MLX4_CMD_SET_PORT	 = 0xc,
	MLX4_CMD_SET_NODE	 = 0x5a,
	MLX4_CMD_QUERY_FUNC	 = 0x56,
	MLX4_CMD_ACCESS_DDR	 = 0x2e,
	MLX4_CMD_MAP_ICM	 = 0xffa,
	MLX4_CMD_UNMAP_ICM	 = 0xff9,
	MLX4_CMD_MAP_ICM_AUX	 = 0xffc,
	MLX4_CMD_UNMAP_ICM_AUX	 = 0xffb,
	MLX4_CMD_SET_ICM_SIZE	 = 0xffd,
	/*master notify fw on finish for slave's flr*/
	MLX4_CMD_INFORM_FLR_DONE = 0x5b,
	MLX4_CMD_GET_OP_REQ      = 0x59,

	/* TPT commands */
	MLX4_CMD_SW2HW_MPT	 = 0xd,
	MLX4_CMD_QUERY_MPT	 = 0xe,
	MLX4_CMD_HW2SW_MPT	 = 0xf,
	MLX4_CMD_READ_MTT	 = 0x10,
	MLX4_CMD_WRITE_MTT	 = 0x11,
	MLX4_CMD_SYNC_TPT	 = 0x2f,

	/* EQ commands */
	MLX4_CMD_MAP_EQ		 = 0x12,
	MLX4_CMD_SW2HW_EQ	 = 0x13,
	MLX4_CMD_HW2SW_EQ	 = 0x14,
	MLX4_CMD_QUERY_EQ	 = 0x15,

	/* CQ commands */
	MLX4_CMD_SW2HW_CQ	 = 0x16,
	MLX4_CMD_HW2SW_CQ	 = 0x17,
	MLX4_CMD_QUERY_CQ	 = 0x18,
	MLX4_CMD_MODIFY_CQ	 = 0x2c,

	/* SRQ commands */
	MLX4_CMD_SW2HW_SRQ	 = 0x35,
	MLX4_CMD_HW2SW_SRQ	 = 0x36,
	MLX4_CMD_QUERY_SRQ	 = 0x37,
	MLX4_CMD_ARM_SRQ	 = 0x40,

	/* QP/EE commands */
	MLX4_CMD_RST2INIT_QP	 = 0x19,
	MLX4_CMD_INIT2RTR_QP	 = 0x1a,
	MLX4_CMD_RTR2RTS_QP	 = 0x1b,
	MLX4_CMD_RTS2RTS_QP	 = 0x1c,
	MLX4_CMD_SQERR2RTS_QP	 = 0x1d,
	MLX4_CMD_2ERR_QP	 = 0x1e,
	MLX4_CMD_RTS2SQD_QP	 = 0x1f,
	MLX4_CMD_SQD2SQD_QP	 = 0x38,
	MLX4_CMD_SQD2RTS_QP	 = 0x20,
	MLX4_CMD_2RST_QP	 = 0x21,
	MLX4_CMD_QUERY_QP	 = 0x22,
	MLX4_CMD_INIT2INIT_QP	 = 0x2d,
	MLX4_CMD_SUSPEND_QP	 = 0x32,
	MLX4_CMD_UNSUSPEND_QP	 = 0x33,
	MLX4_CMD_UPDATE_QP	 = 0x61,
	/* special QP and management commands */
	MLX4_CMD_CONF_SPECIAL_QP = 0x23,
	MLX4_CMD_MAD_IFC	 = 0x24,
	MLX4_CMD_MAD_DEMUX	 = 0x203,

	/* multicast commands */
	MLX4_CMD_READ_MCG	 = 0x25,
	MLX4_CMD_WRITE_MCG	 = 0x26,
	MLX4_CMD_MGID_HASH	 = 0x27,

	/* miscellaneous commands */
	MLX4_CMD_DIAG_RPRT	 = 0x30,
	MLX4_CMD_NOP		 = 0x31,
	MLX4_CMD_ACCESS_MEM	 = 0x2e,
	MLX4_CMD_ACCESS_REG	 = 0x3b,
	MLX4_CMD_SET_VEP	 = 0x52,

	/* Ethernet specific commands */
	MLX4_CMD_SET_VLAN_FLTR	 = 0x47,
	MLX4_CMD_SET_MCAST_FLTR	 = 0x48,
	MLX4_CMD_DUMP_ETH_STATS	 = 0x49,

	/* Communication channel commands */
	MLX4_CMD_ARM_COMM_CHANNEL = 0x57,
	MLX4_CMD_GEN_EQE	 = 0x58,

	/* virtual commands */
	MLX4_CMD_ALLOC_RES	 = 0xf00,
	MLX4_CMD_FREE_RES	 = 0xf01,
	MLX4_CMD_MCAST_ATTACH	 = 0xf05,
	MLX4_CMD_UCAST_ATTACH	 = 0xf06,
	MLX4_CMD_PROMISC         = 0xf08,
	MLX4_CMD_QUERY_FUNC_CAP  = 0xf0a,
	MLX4_CMD_QP_ATTACH	 = 0xf0b,

	/* debug commands */
	MLX4_CMD_QUERY_DEBUG_MSG = 0x2a,
	MLX4_CMD_SET_DEBUG_MSG	 = 0x2b,

	/* statistics commands */
	MLX4_CMD_QUERY_IF_STAT	 = 0X54,
	MLX4_CMD_SET_IF_STAT	 = 0X55,

	/* register/delete flow steering network rules */
	MLX4_QP_FLOW_STEERING_ATTACH = 0x65,
	MLX4_QP_FLOW_STEERING_DETACH = 0x66,
	MLX4_FLOW_STEERING_IB_UC_QP_RANGE = 0x64,
};

enum {
	/* command completed successfully: */
	CMD_STAT_OK		= 0x00,
	/* Internal error (such as a bus error) occurred while
	 * processing command:
	 */
	CMD_STAT_INTERNAL_ERR	= 0x01,
	/* Operation/command not supported or opcode modifier not supported: */
	CMD_STAT_BAD_OP		= 0x02,
	/* Parameter not supported or parameter out of range: */
	CMD_STAT_BAD_PARAM	= 0x03,
	/* System not enabled or bad system state: */
	CMD_STAT_BAD_SYS_STATE	= 0x04,
	/* Attempt to access reserved or unallocaterd resource: */
	CMD_STAT_BAD_RESOURCE	= 0x05,
	/* Requested resource is currently executing a command,
	 * or is otherwise busy:
	 */
	CMD_STAT_RESOURCE_BUSY	= 0x06,
	/* Required capability exceeds device limits: */
	CMD_STAT_EXCEED_LIM	= 0x08,
	/* Resource is not in the appropriate state or ownership: */
	CMD_STAT_BAD_RES_STATE	= 0x09,
	/* Index out of range: */
	CMD_STAT_BAD_INDEX	= 0x0a,
	/* FW image corrupted: */
	CMD_STAT_BAD_NVMEM	= 0x0b,
	/* Error in ICM mapping
	 * (e.g. not enough auxiliary ICM pages to execute command):
	 */
	CMD_STAT_ICM_ERROR	= 0x0c,
	/* Attempt to modify a QP/EE which is not in the presumed state: */
	CMD_STAT_BAD_QP_STATE   = 0x10,
	/* Bad segment parameters (Address/Size): */
	CMD_STAT_BAD_SEG_PARAM	= 0x20,
	/* Memory Region has Memory Windows bound to: */
	CMD_STAT_REG_BOUND	= 0x21,
	/* HCA local attached memory not present: */
	CMD_STAT_LAM_NOT_PRE	= 0x22,
	/* Bad management packet (silently discarded): */
	CMD_STAT_BAD_PKT	= 0x30,
	/* More outstanding CQEs in CQ than new CQ size: */
	CMD_STAT_BAD_SIZE	= 0x40,
	/* Multi Function device support required: */
	CMD_STAT_MULTI_FUNC_REQ	= 0x50,
};

enum {
	MLX4_CMD_TIME_CLASS_A	= 60000,
	MLX4_CMD_TIME_CLASS_B	= 60000,
	MLX4_CMD_TIME_CLASS_C	= 60000,
};

enum {
	MLX4_MAILBOX_SIZE	= 4096,
	MLX4_ACCESS_MEM_ALIGN	= 256,
};

enum {
	/* set port opcode modifiers */
	MLX4_SET_PORT_GENERAL		= 0x0,
	MLX4_SET_PORT_RQP_CALC		= 0x1,
	MLX4_SET_PORT_MAC_TABLE		= 0x2,
	MLX4_SET_PORT_VLAN_TABLE	= 0x3,
	MLX4_SET_PORT_PRIO_MAP		= 0x4,
	MLX4_SET_PORT_GID_TABLE		= 0x5,
	MLX4_SET_PORT_PRIO2TC		= 0x8,
	MLX4_SET_PORT_SCHEDULER		= 0x9,
	MLX4_SET_PORT_VXLAN		= 0xB
};

enum {
	MLX4_CMD_MAD_DEMUX_CONFIG	= 0,
	MLX4_CMD_MAD_DEMUX_QUERY_STATE	= 1,
	MLX4_CMD_MAD_DEMUX_QUERY_REST	= 2, /* Query mad demux restrictions */
};

enum {
	MLX4_CMD_MAD_DEMUX_ENABLED_FLAG	= 1
};

enum {
	MLX4_CMD_WRAPPED,
	MLX4_CMD_NATIVE
};

enum mlx4_config_dev_update_enable {
	MLX4_CONFIG_DEV_UPDATE_ENABLE_VXLAN_UDP_DPORT		= 1UL << 0,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_UDP_UD_ENTROPY	= 1UL << 2,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_UDP_DPORT		= 1UL << 3,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_IPV4_FRAGMENT	= 1UL << 4,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_IPV4_ID	= 1UL << 5,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_IPV4_FLAGS	= 1UL << 6,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_UDP_SPORT	= 1UL << 7,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_UDP_DPORT	= 1UL << 8,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_UDP_LEN	= 1UL << 9,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_ICRC_UDP_CHECKSUM	= 1UL << 10,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_FORMAT_OPTION	= 1UL << 11,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_SX_BW_PORT1		= 1UL << 12,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_SX_BW_PORT2		= 1UL << 13,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_RX_BW_PORT1		= 1UL << 14,
	MLX4_CONFIG_DEV_UPDATE_ENABLE_RROCE_RX_BW_PORT2		= 1UL << 15,
};

enum mlx4_config_dev_rroce_flags {
	MLX4_CONFIG_DEV_RROCE_ICRC_UDP_SPORT		= 1UL << 31,
	MLX4_CONFIG_DEV_RROCE_ICRC_UDP_DPORT		= 1UL << 30,
	MLX4_CONFIG_DEV_RROCE_ICRC_UDP_LEN		= 1UL << 29,
	MLX4_CONFIG_DEV_RROCE_ICRC_UDP_CHECKSUM		= 1UL << 28,
	MLX4_CONFIG_DEV_RROCE_FORMAT_ICRC_IP_HDR	= 1UL << 26,
	MLX4_CONFIG_DEV_RROCE_FORMAT_UDP_CHECKSUM	= 1UL << 25,
	MLX4_CONFIG_DEV_RROCE_FORMAT_UDP_LEN_ICRC	= 1UL << 24,
	MLX4_CONFIG_DEV_RROCE_ICRC_IPV4_FRAGMENT	= 1UL << 20,
	MLX4_CONFIG_DEV_RROCE_ICRC_IPV4_ID		= 1UL << 19,
	MLX4_CONFIG_DEV_RROCE_ICRC_IPV4_ICRC3		= 1UL << 18,
	MLX4_CONFIG_DEV_RROCE_ICRC_IPV4_ICRC2		= 1UL << 17,
	MLX4_CONFIG_DEV_RROCE_ICRC_IPV4_ICRC1		= 1UL << 16,
};

/*
 * MLX4_RX_CSUM_MODE_VAL_NON_TCP_UDP -
 * Receive checksum value is reported in CQE also for non TCP/UDP packets.
 *
 * MLX4_RX_CSUM_MODE_L4 - L4_CSUM bit in CQE is supported.
 *
 * MLX4_RX_CSUM_MODE_IP_OK_IP_NON_TCP_UDP -
 * IP_OK CQE's field is supported also for non TCP/UDP IP packets.
 *
 * MLX4_RX_CSUM_MODE_MULTI_VLAN -
 * Receive Checksum offload is supported for packets with more than 2 vlan headers.
 */
enum mlx4_rx_csum_mode {
	MLX4_RX_CSUM_MODE_VAL_NON_TCP_UDP		= 1UL << 0,
	MLX4_RX_CSUM_MODE_L4				= 1UL << 1,
	MLX4_RX_CSUM_MODE_IP_OK_IP_NON_TCP_UDP		= 1UL << 2,
	MLX4_RX_CSUM_MODE_MULTI_VLAN			= 1UL << 3
};

struct mlx4_rx_csum_params {
	u8	rx_csum_flags_port_1;
	u8	rx_csum_flags_port_2;
};

struct mlx4_dev;

struct mlx4_cmd_context {
	struct completion	done;
	int			result;
	int			next;
	u64			out_param;
	u16			token;
	u8			fw_status;
};

struct mlx4_cmd_mailbox {
	void		       *buf;
	dma_addr_t		dma;
};

struct mlx4_vlan_set_node {
	struct list_head		list;
	u16				vlan_idx;
	u16				vlan_id;
};

int __mlx4_cmd(struct mlx4_dev *dev, u64 in_param, u64 *out_param,
	       int out_is_imm, u32 in_modifier, u8 op_modifier,
	       u16 op, unsigned long timeout, int native);

/* Invoke a command with no output parameter */
static inline int mlx4_cmd(struct mlx4_dev *dev, u64 in_param, u32 in_modifier,
			   u8 op_modifier, u16 op, unsigned long timeout,
			   int native)
{
	return __mlx4_cmd(dev, in_param, NULL, 0, in_modifier,
			  op_modifier, op, timeout, native);
}

/* Invoke a command with an output mailbox */
static inline int mlx4_cmd_box(struct mlx4_dev *dev, u64 in_param, u64 out_param,
			       u32 in_modifier, u8 op_modifier, u16 op,
			       unsigned long timeout, int native)
{
	return __mlx4_cmd(dev, in_param, &out_param, 0, in_modifier,
			  op_modifier, op, timeout, native);
}

/*
 * Invoke a command with an immediate output parameter (and copy the
 * output into the caller's out_param pointer after the command
 * executes).
 */
static inline int mlx4_cmd_imm(struct mlx4_dev *dev, u64 in_param, u64 *out_param,
			       u32 in_modifier, u8 op_modifier, u16 op,
			       unsigned long timeout, int native)
{
	return __mlx4_cmd(dev, in_param, out_param, 1, in_modifier,
			  op_modifier, op, timeout, native);
}

struct mlx4_cmd_mailbox *mlx4_alloc_cmd_mailbox(struct mlx4_dev *dev);
void mlx4_free_cmd_mailbox(struct mlx4_dev *dev, struct mlx4_cmd_mailbox *mailbox);

int mlx4_get_vf_statistics(struct mlx4_dev *dev, int port, int vf,
			   struct net_device_stats *link_stats);

#define MLX4_MAX_VLAN_SET_SIZE	10

ssize_t mlx4_get_vf_vlan_set(struct mlx4_dev *dev, int port, int vf, char *buf);
int mlx4_set_vf_vlan_next(struct mlx4_dev *dev, int port, int vf, u16 vlan_id);
int mlx4_reset_vlan_policy(struct mlx4_dev *dev, int port, int vf);
int mlx4_vlan_index_exists(struct list_head *vlan_list, u16 vlan_id);
int mlx4_vlan_blocked(struct mlx4_dev *dev, int port, int slave, u16 vlan_id);
u32 mlx4_comm_get_version(void);
int mlx4_set_vf_mac(struct mlx4_dev *dev, int port, int vf, u8 *mac);
int mlx4_set_vf_vlan(struct mlx4_dev *dev, int port, int vf, u16 vlan, u8 qos);
int mlx4_set_vf_spoofchk(struct mlx4_dev *dev, int port, int vf, bool setting);
#ifdef CONFIG_COMPAT_NDO_VF_MAC_VLAN
int mlx4_get_vf_config(struct mlx4_dev *dev, int port, int vf, struct ifla_vf_info *ivf);
#endif
int mlx4_set_vf_link_state(struct mlx4_dev *dev, int port, int vf, int link_state);
int mlx4_get_vf_link_state(struct mlx4_dev *dev, int port, int vf);
int mlx4_config_dev_rx_flags_retrieval(struct mlx4_dev *dev,
				       struct mlx4_rx_csum_params *params);
/*
 * mlx4_get_slave_default_vlan -
 * retrun true if VST ( default vlan)
 * if VST will fill vlan & qos (if not NULL)
 */
bool mlx4_get_slave_default_vlan(struct mlx4_dev *dev, int port, int slave, u16 *vlan, u8 *qos);
int mlx4_status_to_errno(u8 status);
void report_internal_err_comm_event(struct mlx4_dev *dev);

#define MLX4_COMM_GET_IF_REV(cmd_chan_ver) (u8)((cmd_chan_ver) >> 8)
#define COMM_CHAN_EVENT_INTERNAL_ERR (1 << 17)

enum mlx4_ptys_proto {
	MLX4_PTYS_IB = 1<<0,
	MLX4_PTYS_EN = 1<<2,
};

enum mlx4_access_reg_method {
	MLX4_ACCESS_REG_QUERY = 0x1,
	MLX4_ACCESS_REG_WRITE = 0x2,
};

struct mlx4_ptys_reg {
	u8 resrvd1;
	u8 local_port;
	u8 resrvd2;
	u8 proto_mask;
	__be32 resrvd3[2];
	__be32 eth_proto_cap;
	__be16 ib_width_cap;
	__be16 ib_speed_cap;
	__be32 resrvd4;
	__be32 eth_proto_admin;
	__be16 ib_width_admin;
	__be16 ib_speed_admin;
	__be32 resrvd5;
	__be32 eth_proto_oper;
	__be16 ib_width_oper;
	__be16 ib_speed_oper;
	__be32 resrvd6;
	__be32 eth_proto_lp_adv;
} __attribute__((__packed__));

int mlx4_ACCESS_PTYS_REG(struct mlx4_dev *dev,
			 enum mlx4_access_reg_method method,
			 struct mlx4_ptys_reg *ptys_reg);

#endif /* MLX4_CMD_H */
