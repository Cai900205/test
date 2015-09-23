/*
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <netinet/in.h>
#include <endian.h>
#include <byteswap.h>
#include <inttypes.h>

#include <infiniband/verbs.h>
#include <infiniband/driver.h>
#include <infiniband/arch.h>

static int verbose;

static int null_gid(union ibv_gid *gid)
{
	return !(gid->raw[8] | gid->raw[9] | gid->raw[10] | gid->raw[11] |
		 gid->raw[12] | gid->raw[13] | gid->raw[14] | gid->raw[15]);
}

static const char *guid_str(uint64_t node_guid, char *str)
{
	node_guid = ntohll(node_guid);
	sprintf(str, "%04x:%04x:%04x:%04x",
		(unsigned) (node_guid >> 48) & 0xffff,
		(unsigned) (node_guid >> 32) & 0xffff,
		(unsigned) (node_guid >> 16) & 0xffff,
		(unsigned) (node_guid >>  0) & 0xffff);
	return str;
}

static const char *transport_str(enum ibv_transport_type transport)
{
	switch (transport) {
	case IBV_TRANSPORT_IB:		return "InfiniBand";
	case IBV_TRANSPORT_IWARP:	return "iWARP";
	case IBV_EXP_TRANSPORT_SCIF:	return "SCIF";
	default:			return "invalid transport";
	}
}

static const char *port_state_str(enum ibv_port_state pstate)
{
	switch (pstate) {
	case IBV_PORT_DOWN:   return "PORT_DOWN";
	case IBV_PORT_INIT:   return "PORT_INIT";
	case IBV_PORT_ARMED:  return "PORT_ARMED";
	case IBV_PORT_ACTIVE: return "PORT_ACTIVE";
	default:              return "invalid state";
	}
}

static const char *port_phy_state_str(uint8_t phys_state)
{
	switch (phys_state) {
	case 1:  return "SLEEP";
	case 2:  return "POLLING";
	case 3:  return "DISABLED";
	case 4:  return "PORT_CONFIGURATION TRAINNING";
	case 5:  return "LINK_UP";
	case 6:  return "LINK_ERROR_RECOVERY";
	case 7:  return "PHY TEST";
	default: return "invalid physical state";
	}
}

static const char *exp_atomic_cap_str(enum ibv_exp_atomic_cap exp_atom_cap)
{
	switch (exp_atom_cap) {
	case IBV_EXP_ATOMIC_NONE: return "ATOMIC_NONE";
	case IBV_EXP_ATOMIC_HCA:  return "ATOMIC_HCA";
	case IBV_EXP_ATOMIC_GLOB: return "ATOMIC_GLOB";
	case IBV_EXP_ATOMIC_HCA_REPLY_BE: return "ATOMIC_HCA_REPLY_BE";
	default:              return "invalid atomic capability";
	}
}

static const char *mtu_str(enum ibv_mtu max_mtu)
{
	switch (max_mtu) {
	case IBV_MTU_256:  return "256";
	case IBV_MTU_512:  return "512";
	case IBV_MTU_1024: return "1024";
	case IBV_MTU_2048: return "2048";
	case IBV_MTU_4096: return "4096";
	default:           return "invalid MTU";
	}
}

static const char *width_str(uint8_t width)
{
	switch (width) {
	case 1:  return "1";
	case 2:  return "4";
	case 4:  return "8";
	case 8:  return "12";
	default: return "invalid width";
	}
}

static const char *speed_str(uint8_t speed)
{
	switch (speed) {
	case 1:  return "2.5 Gbps";
	case 2:  return "5.0 Gbps";

	case 4:  /* fall through */
	case 8:  return "10.0 Gbps";

	case 16: return "14.0 Gbps";
	case 32: return "25.0 Gbps";
	default: return "invalid speed";
	}
}

static const char *vl_str(uint8_t vl_num)
{
	switch (vl_num) {
	case 1:  return "1";
	case 2:  return "2";
	case 3:  return "4";
	case 4:  return "8";
	case 5:  return "15";
	default: return "invalid value";
	}
}

static int print_all_port_gids(struct ibv_context *ctx, uint8_t port_num, int tbl_len)
{
	union ibv_gid gid;
	int rc = 0;
	int i;

	for (i = 0; i < tbl_len; i++) {
		rc = ibv_query_gid(ctx, port_num, i, &gid);
		if (rc) {
			fprintf(stderr, "Failed to query gid to port %d, index %d\n",
			       port_num, i);
			return rc;
		}
		if (!null_gid(&gid))
			printf("\t\t\tGID[%3d]:\t\t%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
			       i,
			       gid.raw[ 0], gid.raw[ 1],
			       gid.raw[ 2], gid.raw[ 3],
			       gid.raw[ 4], gid.raw[ 5],
			       gid.raw[ 6], gid.raw[ 7],
			       gid.raw[ 8], gid.raw[ 9],
			       gid.raw[10], gid.raw[11],
			       gid.raw[12], gid.raw[13],
			       gid.raw[14], gid.raw[15]);
	}
	return rc;
}

static const char *link_layer_str(uint8_t link_layer)
{
	switch (link_layer) {
	case IBV_LINK_LAYER_UNSPECIFIED:
	case IBV_LINK_LAYER_INFINIBAND:
		return "InfiniBand";
	case IBV_LINK_LAYER_ETHERNET:
		return "Ethernet";
	case IBV_EXP_LINK_LAYER_SCIF:
		return "SCIF";
	default:
		return "Unknown";
	}
}

static void print_caps(int caps)
{
	int unknown_flags = ~(IBV_DEVICE_RESIZE_MAX_WR |
			      IBV_DEVICE_BAD_PKEY_CNTR |
			      IBV_DEVICE_BAD_QKEY_CNTR |
			      IBV_DEVICE_RAW_MULTI |
			      IBV_DEVICE_AUTO_PATH_MIG |
			      IBV_DEVICE_CHANGE_PHY_PORT |
			      IBV_DEVICE_UD_AV_PORT_ENFORCE |
			      IBV_DEVICE_CURR_QP_STATE_MOD |
			      IBV_DEVICE_SHUTDOWN_PORT |
			      IBV_DEVICE_INIT_TYPE |
			      IBV_DEVICE_PORT_ACTIVE_EVENT |
			      IBV_DEVICE_SYS_IMAGE_GUID |
			      IBV_DEVICE_RC_RNR_NAK_GEN |
			      IBV_DEVICE_SRQ_RESIZE |
			      IBV_DEVICE_N_NOTIFY_CQ |
			      IBV_DEVICE_XRC);

	if (caps & IBV_DEVICE_RESIZE_MAX_WR)
		printf("\t\t\t\t\tRESIZE_MAX_WR\n");
	if (caps & IBV_DEVICE_BAD_PKEY_CNTR)
		printf("\t\t\t\t\tBAD_PKEY_CNTR\n");
	if (caps & IBV_DEVICE_BAD_QKEY_CNTR)
		printf("\t\t\t\t\tBAD_QKEY_CNTR\n");
	if (caps & IBV_DEVICE_RAW_MULTI)
		printf("\t\t\t\t\tRAW_MULTI\n");
	if (caps & IBV_DEVICE_AUTO_PATH_MIG)
		printf("\t\t\t\t\tAUTO_PATH_MIG\n");
	if (caps & IBV_DEVICE_CHANGE_PHY_PORT)
		printf("\t\t\t\t\tCHANGE_PHY_PORT\n");
	if (caps & IBV_DEVICE_UD_AV_PORT_ENFORCE)
		printf("\t\t\t\t\tUD_AV_PORT_ENFORCE\n");
	if (caps & IBV_DEVICE_CURR_QP_STATE_MOD)
		printf("\t\t\t\t\tCURR_QP_STATE_MOD\n");
	if (caps & IBV_DEVICE_SHUTDOWN_PORT)
		printf("\t\t\t\t\tSHUTDOWN_PORT\n");
	if (caps & IBV_DEVICE_INIT_TYPE)
		printf("\t\t\t\t\tINIT_TYPE\n");
	if (caps & IBV_DEVICE_PORT_ACTIVE_EVENT)
		printf("\t\t\t\t\tPORT_ACTIVE_EVENT\n");
	if (caps & IBV_DEVICE_SYS_IMAGE_GUID)
		printf("\t\t\t\t\tSYS_IMAGE_GUID\n");
	if (caps & IBV_DEVICE_RC_RNR_NAK_GEN)
		printf("\t\t\t\t\tRC_RNR_NAK_GEN\n");
	if (caps & IBV_DEVICE_SRQ_RESIZE)
		printf("\t\t\t\t\tSRQ_RESIZE\n");
	if (caps & IBV_DEVICE_N_NOTIFY_CQ)
		printf("\t\t\t\t\tN_NOTIFY_CQ\n");
	if (caps & IBV_DEVICE_XRC)
		printf("\t\t\t\t\tXRC\n");
	if (caps & unknown_flags)
		printf("\t\t\t\t\tUnknown flags: 0x%08x\n", caps & unknown_flags);
}

static void print_caps_exp(uint64_t caps)
{
	uint64_t unknown_flags = ~(IBV_EXP_DEVICE_DC_TRANSPORT |
				   IBV_EXP_DEVICE_QPG |
				   IBV_EXP_DEVICE_UD_RSS |
				   IBV_EXP_DEVICE_UD_TSS |
				   IBV_EXP_DEVICE_MEM_WINDOW |
				   IBV_EXP_DEVICE_MEM_MGT_EXTENSIONS |
				   IBV_EXP_DEVICE_MW_TYPE_2A |
				   IBV_EXP_DEVICE_MW_TYPE_2B |
				   IBV_EXP_DEVICE_CROSS_CHANNEL |
				   IBV_EXP_DEVICE_MANAGED_FLOW_STEERING |
				   IBV_EXP_DEVICE_MR_ALLOCATE |
				   IBV_EXP_DEVICE_MR_ALLOCATE |
				   IBV_EXP_DEVICE_EXT_ATOMICS |
				   IBV_EXP_DEVICE_NOP |
				   IBV_EXP_DEVICE_UMR |
				   IBV_EXP_DEVICE_ODP);

	if (caps & IBV_EXP_DEVICE_DC_TRANSPORT)
		printf("\t\t\t\t\tEXP_DC_TRANSPORT\n");
	if (caps & IBV_EXP_DEVICE_QPG)
		printf("\t\t\t\t\tEXP_DEVICE_QPG\n");
	if (caps & IBV_EXP_DEVICE_UD_RSS)
		printf("\t\t\t\t\tEXP_UD_RSS\n");
	if (caps & IBV_EXP_DEVICE_UD_TSS)
		printf("\t\t\t\t\tEXP_UD_TSS\n");
	if (caps & IBV_EXP_DEVICE_MEM_WINDOW)
		printf("\t\t\t\t\tEXP_MEM_WINDOW\n");
	if (caps & IBV_EXP_DEVICE_MEM_MGT_EXTENSIONS)
		printf("\t\t\t\t\tEXP_MEM_MGT_EXTENSIONS\n");
	if (caps & IBV_EXP_DEVICE_MW_TYPE_2A)
		printf("\t\t\t\t\tEXP_MW_TYPE_2A\n");
	if (caps & IBV_EXP_DEVICE_MW_TYPE_2B)
		printf("\t\t\t\t\tEXP_MW_TYPE_2B\n");
	if (caps & IBV_EXP_DEVICE_CROSS_CHANNEL)
		printf("\t\t\t\t\tEXP_CROSS_CHANNEL\n");
	if (caps & IBV_EXP_DEVICE_MANAGED_FLOW_STEERING)
		printf("\t\t\t\t\tEXP_MANAGED_FLOW_STEERING\n");
	if (caps & IBV_EXP_DEVICE_MR_ALLOCATE)
		printf("\t\t\t\t\tEXP_MR_ALLOCATE\n");
	if (caps & IBV_EXP_DEVICE_SHARED_MR)
		printf("\t\t\t\t\tEXP_SHARED_MR\n");
	if (caps & IBV_EXP_DEVICE_EXT_ATOMICS)
		printf("\t\t\t\t\tEXT_ATOMICS\n");
	if (caps & IBV_EXP_DEVICE_NOP)
		printf("\t\t\t\t\tEXT_SEND NOP\n");
	if (caps & IBV_EXP_DEVICE_UMR)
		printf("\t\t\t\t\tEXP_UMR\n");
	if (caps & IBV_EXP_DEVICE_ODP)
		printf("\t\t\t\t\tEXP_ODP\n");
	if (caps & unknown_flags)
		printf("\t\t\t\t\tUnknown flags: 0x%" PRIX64 "\n", caps & unknown_flags);
}

void print_odp_trans_caps(uint32_t trans)
{
        uint32_t unknown_transport_caps = ~(IBV_EXP_ODP_SUPPORT_SEND |
					    IBV_EXP_ODP_SUPPORT_RECV |
					    IBV_EXP_ODP_SUPPORT_WRITE |
					    IBV_EXP_ODP_SUPPORT_READ |
					    IBV_EXP_ODP_SUPPORT_ATOMIC |
					    IBV_EXP_ODP_SUPPORT_SRQ_RECV);

	if (!trans)
		printf("\t\t\t\t\tNO SUPPORT\n");
	else {
		if (trans & IBV_EXP_ODP_SUPPORT_SEND)
			printf("\t\t\t\t\tSUPPORT_SEND\n");
		if (trans & IBV_EXP_ODP_SUPPORT_RECV)
			printf("\t\t\t\t\tSUPPORT_RECV\n");
		if (trans & IBV_EXP_ODP_SUPPORT_WRITE)
			printf("\t\t\t\t\tSUPPORT_WRITE\n");
		if (trans & IBV_EXP_ODP_SUPPORT_READ)
			printf("\t\t\t\t\tSUPPORT_READ\n");
		if (trans & IBV_EXP_ODP_SUPPORT_ATOMIC)
			printf("\t\t\t\t\tSUPPORT_ATOMIC\n");
		if (trans & IBV_EXP_ODP_SUPPORT_SRQ_RECV)
			printf("\t\t\t\t\tSUPPORT_SRQ_RECV\n");
		if (trans & unknown_transport_caps)
			printf("\t\t\t\t\tUnkown flags: 0x%" PRIX32 "\n",
			       trans & unknown_transport_caps);
	}
}

void print_odp_caps(struct ibv_exp_odp_caps caps)
{
	uint64_t unknown_general_caps = ~(IBV_EXP_ODP_SUPPORT);
	
	/* general odp caps */
	printf("\tgeneral_odp_caps:\n");
	if (caps.general_odp_caps & IBV_EXP_ODP_SUPPORT)
		printf("\t\t\t\t\tODP_SUPPORT\n");
	if (caps.general_odp_caps & unknown_general_caps)
		printf("\t\t\t\t\tUnkown flags: 0x%" PRIX64 "\n",
		       caps.general_odp_caps & unknown_general_caps);

	/* RC transport */
	printf("\trc_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.rc_odp_caps);
	printf("\tuc_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.uc_odp_caps);
	printf("\tud_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.ud_odp_caps);
	printf("\tdc_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.dc_odp_caps);
	printf("\txrc_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.xrc_odp_caps);
	printf("\traw_eth_odp_caps:\n");
	print_odp_trans_caps(caps.per_transport_caps.raw_eth_odp_caps);
}

static int print_hca_cap(struct ibv_device *ib_dev, uint8_t ib_port)
{
	struct ibv_context *ctx;
	struct ibv_exp_device_attr device_attr;
	struct ibv_port_attr port_attr;
	int rc = 0;
	uint8_t port;
	char buf[256];

	ctx = ibv_open_device(ib_dev);
	if (!ctx) {
		fprintf(stderr, "Failed to open device\n");
		rc = 1;
		goto cleanup;
	}

	memset(&device_attr, 0, sizeof(device_attr));
	device_attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED - 1;

	if (ibv_exp_query_device(ctx, &device_attr)) {
		fprintf(stderr, "Failed to query device props");
		rc = 2;
		goto cleanup;
	}

	printf("hca_id:\t%s\n", ibv_get_device_name(ib_dev));
	printf("\ttransport:\t\t\t%s (%d)\n",
	       transport_str(ib_dev->transport_type), ib_dev->transport_type);
	if (strlen(device_attr.fw_ver))
		printf("\tfw_ver:\t\t\t\t%s\n", device_attr.fw_ver);
	printf("\tnode_guid:\t\t\t%s\n", guid_str(device_attr.node_guid, buf));
	printf("\tsys_image_guid:\t\t\t%s\n", guid_str(device_attr.sys_image_guid, buf));
	printf("\tvendor_id:\t\t\t0x%04x\n", device_attr.vendor_id);
	printf("\tvendor_part_id:\t\t\t%d\n", device_attr.vendor_part_id);
	printf("\thw_ver:\t\t\t\t0x%X\n", device_attr.hw_ver);

	if (ibv_read_sysfs_file(ib_dev->ibdev_path, "board_id", buf, sizeof buf) > 0)
		printf("\tboard_id:\t\t\t%s\n", buf);

	printf("\tphys_port_cnt:\t\t\t%d\n", device_attr.phys_port_cnt);

	if (verbose) {
		printf("\tmax_mr_size:\t\t\t0x%llx\n",
		       (unsigned long long) device_attr.max_mr_size);
		printf("\tpage_size_cap:\t\t\t0x%llx\n",
		       (unsigned long long) device_attr.page_size_cap);
		printf("\tmax_qp:\t\t\t\t%d\n", device_attr.max_qp);
		printf("\tmax_qp_wr:\t\t\t%d\n", device_attr.max_qp_wr);
		printf("\tdevice_cap_flags:\t\t0x%08x\n",
		       (int)(device_attr.exp_device_cap_flags & (IBV_EXP_START_FLAG - 1)));
		print_caps(device_attr.exp_device_cap_flags & (IBV_EXP_START_FLAG - 1));
		printf("\tdevice_cap_exp_flags:\t\t0x%" PRIX64 "\n",
		       device_attr.exp_device_cap_flags & ~(IBV_EXP_START_FLAG - 1));
		print_caps_exp(device_attr.exp_device_cap_flags & ~(IBV_EXP_START_FLAG - 1));
		printf("\tmax_sge:\t\t\t%d\n", device_attr.max_sge);
		printf("\tmax_sge_rd:\t\t\t%d\n", device_attr.max_sge_rd);
		printf("\tmax_cq:\t\t\t\t%d\n", device_attr.max_cq);
		printf("\tmax_cqe:\t\t\t%d\n", device_attr.max_cqe);
		printf("\tmax_mr:\t\t\t\t%d\n", device_attr.max_mr);
		printf("\tmax_pd:\t\t\t\t%d\n", device_attr.max_pd);
		printf("\tmax_qp_rd_atom:\t\t\t%d\n", device_attr.max_qp_rd_atom);
		printf("\tmax_ee_rd_atom:\t\t\t%d\n", device_attr.max_ee_rd_atom);
		printf("\tmax_res_rd_atom:\t\t%d\n", device_attr.max_res_rd_atom);
		printf("\tmax_qp_init_rd_atom:\t\t%d\n", device_attr.max_qp_init_rd_atom);
		printf("\tmax_ee_init_rd_atom:\t\t%d\n", device_attr.max_ee_init_rd_atom);
		printf("\tatomic_cap:\t\t\t%s (%d)\n",
		       exp_atomic_cap_str(device_attr.exp_atomic_cap),
		       device_attr.exp_atomic_cap);
		printf("\tlog atomic arg sizes (mask)\t\t%" PRIx64 "\n", device_attr.ext_atom.log_atomic_arg_sizes);
		printf("\tmax fetch and add bit boundary\t%d\n", device_attr.ext_atom.max_fa_bit_boundary);
		printf("\tlog max atomic inline\t\t%d\n", device_attr.ext_atom.log_max_atomic_inline);
		printf("\tmax_ee:\t\t\t\t%d\n", device_attr.max_ee);
		printf("\tmax_rdd:\t\t\t%d\n", device_attr.max_rdd);
		printf("\tmax_mw:\t\t\t\t%d\n", device_attr.max_mw);
		printf("\tmax_raw_ipv6_qp:\t\t%d\n", device_attr.max_raw_ipv6_qp);
		printf("\tmax_raw_ethy_qp:\t\t%d\n", device_attr.max_raw_ethy_qp);
		printf("\tmax_mcast_grp:\t\t\t%d\n", device_attr.max_mcast_grp);
		printf("\tmax_mcast_qp_attach:\t\t%d\n", device_attr.max_mcast_qp_attach);
		printf("\tmax_total_mcast_qp_attach:\t%d\n",
		       device_attr.max_total_mcast_qp_attach);
		printf("\tmax_ah:\t\t\t\t%d\n", device_attr.max_ah);
		printf("\tmax_fmr:\t\t\t%d\n", device_attr.max_fmr);
		if (device_attr.max_fmr)
			printf("\tmax_map_per_fmr:\t\t%d\n", device_attr.max_map_per_fmr);
		printf("\tmax_srq:\t\t\t%d\n", device_attr.max_srq);
		if (device_attr.max_srq) {
			printf("\tmax_srq_wr:\t\t\t%d\n", device_attr.max_srq_wr);
			printf("\tmax_srq_sge:\t\t\t%d\n", device_attr.max_srq_sge);
		}
		printf("\tmax_pkeys:\t\t\t%d\n", device_attr.max_pkeys);
		printf("\tlocal_ca_ack_delay:\t\t%d\n", device_attr.local_ca_ack_delay);
		printf("\thca_core_clock:\t\t\t%" PRIu64 "\n",
		       device_attr.hca_core_clock);
		printf("\tmax_klm_list_size:\t\t%d\n", device_attr.umr_caps.max_klm_list_size);
		printf("\tmax_send_wqe_inline_klms:\t%d\n", device_attr.umr_caps.max_send_wqe_inline_klms);
		printf("\tmax_umr_recursion_depth:\t%d\n", device_attr.umr_caps.max_umr_recursion_depth);
		printf("\tmax_umr_stride_dimension:\t%d\n", device_attr.umr_caps.max_umr_stride_dimension);
		print_odp_caps(device_attr.odp_caps);
		printf("\tmax_dct:\t\t\t%d\n", device_attr.max_dct);
	}

	for (port = 1; port <= device_attr.phys_port_cnt; ++port) {
		/* if in the command line the user didn't ask for info about this port */
		if ((ib_port) && (port != ib_port))
			continue;

		rc = ibv_query_port(ctx, port, &port_attr);
		if (rc) {
			fprintf(stderr, "Failed to query port %u props\n", port);
			goto cleanup;
		}
		printf("\t\tport:\t%d\n", port);
		printf("\t\t\tstate:\t\t\t%s (%d)\n",
		       port_state_str(port_attr.state), port_attr.state);
		printf("\t\t\tmax_mtu:\t\t%s (%d)\n",
		       mtu_str(port_attr.max_mtu), port_attr.max_mtu);
		printf("\t\t\tactive_mtu:\t\t%s (%d)\n",
		       mtu_str(port_attr.active_mtu), port_attr.active_mtu);
		printf("\t\t\tsm_lid:\t\t\t%d\n", port_attr.sm_lid);
		printf("\t\t\tport_lid:\t\t%d\n", port_attr.lid);
		printf("\t\t\tport_lmc:\t\t0x%02x\n", port_attr.lmc);
		printf("\t\t\tlink_layer:\t\t%s\n",
					link_layer_str(port_attr.link_layer));

		if (verbose) {
			printf("\t\t\tmax_msg_sz:\t\t0x%x\n", port_attr.max_msg_sz);
			printf("\t\t\tport_cap_flags:\t\t0x%08x\n", port_attr.port_cap_flags);
			printf("\t\t\tmax_vl_num:\t\t%s (%d)\n",
			       vl_str(port_attr.max_vl_num), port_attr.max_vl_num);
			printf("\t\t\tbad_pkey_cntr:\t\t0x%x\n", port_attr.bad_pkey_cntr);
			printf("\t\t\tqkey_viol_cntr:\t\t0x%x\n", port_attr.qkey_viol_cntr);
			printf("\t\t\tsm_sl:\t\t\t%d\n", port_attr.sm_sl);
			printf("\t\t\tpkey_tbl_len:\t\t%d\n", port_attr.pkey_tbl_len);
			printf("\t\t\tgid_tbl_len:\t\t%d\n", port_attr.gid_tbl_len);
			printf("\t\t\tsubnet_timeout:\t\t%d\n", port_attr.subnet_timeout);
			printf("\t\t\tinit_type_reply:\t%d\n", port_attr.init_type_reply);
			printf("\t\t\tactive_width:\t\t%sX (%d)\n",
			       width_str(port_attr.active_width), port_attr.active_width);
			printf("\t\t\tactive_speed:\t\t%s (%d)\n",
			       speed_str(port_attr.active_speed), port_attr.active_speed);
			printf("\t\t\tphys_state:\t\t%s (%d)\n",
			       port_phy_state_str(port_attr.phys_state), port_attr.phys_state);

			if (print_all_port_gids(ctx, port, port_attr.gid_tbl_len))
				goto cleanup;
		}
		printf("\n");
	}
cleanup:
	if (ctx)
		if (ibv_close_device(ctx)) {
			fprintf(stderr, "Failed to close device");
			rc = 3;
		}
	return rc;
}

static void usage(const char *argv0)
{
	printf("Usage: %s             print the ca attributes\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -d, --ib-dev=<dev>     use IB device <dev> (default all devices found)\n");
	printf("  -i, --ib-port=<port>   use port <port> of IB device (default 0: all ports)\n");
	printf("  -l, --list             print only the IB devices names\n");
	printf("  -v, --verbose          print all the attributes of the IB device(s)\n");
}

int main(int argc, char *argv[])
{
	char *ib_devname = NULL;
	int ret = 0;
	struct ibv_device **dev_list, **orig_dev_list;
	int num_of_hcas;
	int ib_port = 0;

	/* parse command line options */
	while (1) {
		int c;
		static struct option long_options[] = {
			{ .name = "ib-dev",   .has_arg = 1, .val = 'd' },
			{ .name = "ib-port",  .has_arg = 1, .val = 'i' },
			{ .name = "list",     .has_arg = 0, .val = 'l' },
			{ .name = "verbose",  .has_arg = 0, .val = 'v' },
			{ 0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "d:i:lv", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'd':
			ib_devname = strdup(optarg);
			break;

		case 'i':
			ib_port = strtol(optarg, NULL, 0);
			if (ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'v':
			verbose = 1;
			break;

		case 'l':
			dev_list = orig_dev_list = ibv_get_device_list(&num_of_hcas);
			if (!dev_list) {
				perror("Failed to get IB devices list");
				return -1;
			}

			printf("%d HCA%s found:\n", num_of_hcas,
			       num_of_hcas != 1 ? "s" : "");

			while (*dev_list) {
				printf("\t%s\n", ibv_get_device_name(*dev_list));
				++dev_list;
			}

			printf("\n");

			ibv_free_device_list(orig_dev_list);

			return 0;

		default:
			usage(argv[0]);
			return -1;
		}
	}

	dev_list = orig_dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
		return -1;
	}

	if (ib_devname) {
		while (*dev_list) {
			if (!strcmp(ibv_get_device_name(*dev_list), ib_devname))
				break;
			++dev_list;
		}

		if (!*dev_list) {
			fprintf(stderr, "IB device '%s' wasn't found\n", ib_devname);
			return -1;
		}

		ret |= print_hca_cap(*dev_list, ib_port);
	} else {
		if (!*dev_list) {
			fprintf(stderr, "No IB devices found\n");
			return -1;
		}

		while (*dev_list) {
			ret |= print_hca_cap(*dev_list, ib_port);
			++dev_list;
		}
	}

	if (ib_devname)
		free(ib_devname);

	ibv_free_device_list(orig_dev_list);

	return ret;
}
