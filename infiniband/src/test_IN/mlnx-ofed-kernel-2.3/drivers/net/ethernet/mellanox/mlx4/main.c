/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005, 2006, 2007, 2008 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/io-mapping.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/kmod.h>
#include <linux/printk.h>

#include <linux/mlx4/device.h>
#include <linux/mlx4/doorbell.h>

#include "mlx4.h"
#include "fw.h"
#include "icm.h"
#include "mlx4_stats.h"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("Mellanox ConnectX HCA low-level driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

struct workqueue_struct *mlx4_wq;

#ifdef CONFIG_MLX4_DEBUG

int mlx4_debug_level = 0;
module_param_named(debug_level, mlx4_debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Enable debug tracing if > 0");

#endif /* CONFIG_MLX4_DEBUG */

#ifdef CONFIG_PCI_MSI

static int msi_x = 1;
module_param(msi_x, int, 0444);
MODULE_PARM_DESC(msi_x, "0 - don't use MSI-X, 1 - use MSI-X, >1 - limit number of MSI-X irqs to msi_x");

#else /* CONFIG_PCI_MSI */

#define msi_x (0)

#endif /* CONFIG_PCI_MSI */

static int enable_sys_tune = 0;
module_param(enable_sys_tune, int, 0444);
MODULE_PARM_DESC(enable_sys_tune, "Tune the cpu's for better performance (default 0)");

int mlx4_blck_lb = 1;
module_param_named(block_loopback, mlx4_blck_lb, int, 0644);
MODULE_PARM_DESC(block_loopback, "Block multicast loopback packets if > 0 "
				 "(default: 1)");

#define MLX4_ROCE_1_5_DEF_PROTO 0xfe
#define MLX4_ROCE_2_DEF_PROTO	1021

int mlx4_roce_proto_config;
module_param_named(rr_proto, mlx4_roce_proto_config, int, 0444);
MODULE_PARM_DESC(rr_proto, "IP next protocol for RoCEv1.5 or destination port for RoCEv2. Seeting value 0 indicates using driver default values");


enum {
	DEFAULT_DOMAIN	= 0,
	BDF_STR_SIZE	= 8, /* bb:dd.f- */
	DBDF_STR_SIZE	= 13 /* mmmm:bb:dd.f- */
};

enum {
	NUM_VFS,
	PROBE_VF,
	PORT_TYPE_ARRAY,
	ROCE_MODE
};

enum {
	VALID_DATA,
	INVALID_DATA,
	INVALID_STR
};

struct param_data {
	int				id;
	struct mlx4_dbdf2val_lst	dbdf2val;
};

static struct param_data roce_mode = {
	.id		= ROCE_MODE,
	.dbdf2val = {
		.name		= "roce_mode param",
		.num_vals	= 1,
		.def_val	= {MLX4_ROCE_MODE_1},
		.range		= {MLX4_ROCE_MODE_1, MLX4_ROCE_MODE_2},
		.num_inval_vals = 0
	}
};
module_param_string(roce_mode, roce_mode.dbdf2val.str,
		    sizeof(roce_mode.dbdf2val.str), 0444);
MODULE_PARM_DESC(roce_mode,
		 "A single value (e.g. 1) to define uniform RoCE_mode value for all devices\n"
		 "\t\tor a string to map device function numbers to their RoCE mode value (e.g. '0000:04:00.0-5,002b:1c:0b.a-1').\n"
		 "\t\tAllowed values are 0 for RoCEv1 (default), 1 for RoCEv1.5 and 2 for RoCEv2");

static struct param_data num_vfs = {
	.id		= NUM_VFS,
	.dbdf2val = {
		.name		= "num_vfs param",
		.num_vals	= 3,
		.def_val	= {0},
		.range		= {0, MLX4_MAX_NUM_VF},
		.num_inval_vals = 0
	}
};
module_param_string(num_vfs, num_vfs.dbdf2val.str,
		    sizeof(num_vfs.dbdf2val.str), 0444);
MODULE_PARM_DESC(num_vfs,
		 "Either single value (e.g. '5') or triplet (e.g. '10,11,12') to define uniform num_vfs value for all devices functions.\n"
		 "\t\tIf a single value is given, this value will be used in order to define <num_vfs> dual ports virtual functions.\n"
		 "\t\tIf a triplet <a,b,c> is given, <a> single port virtual functions are defined on port1, <b> single port\n"
		 "\t\tvirtual functions are defined on port2 and <c> dual port virtual functions are defined.\n"
		 "\t\tAlternatively, a string to map device function numbers to their num_vfs values\n"
		 "\t\t (e.g. '0000:04:00.0-5,002b:1c:0b.a-15;2;4') could be given.\n"
		 "\t\tHexadecimal digits for the device function (e.g. 002b:1c:0b.a) and decimal or triplet for num_vfs value\n"
		 "\t\t(e.g. 15 or 1;2;3).");

static struct param_data probe_vf = {
	.id		= PROBE_VF,
	.dbdf2val = {
		.name		= "probe_vf param",
		.num_vals	= 3,
		.def_val	= {0},
		.range		= {0, MLX4_MAX_NUM_VF},
		.num_inval_vals = 0
	}
};
module_param_string(probe_vf, probe_vf.dbdf2val.str,
		    sizeof(probe_vf.dbdf2val.str), 0444);
MODULE_PARM_DESC(probe_vf,
		 "Either single value (e.g. '3') or triplet (e.g '1,2,3') to define uniform number of VFs to probe by the pf\n"
		 "\t\tdriver for all devices functions.\n"
		 "\t\tIf a single value is given, this value will be used in order to define <probe_vf> probed dual ports virtual\n"
		 "\t\tfunctions. If a triplet <a,b,c> is given, <a> single port virtual functions are probed on port1, <b> single port\n"
		 "\t\tvirtual functions are probed on port2 and <c> dual port virtual functions are probed.\n"
		 "\t\tAlternatively, a string to map device function numbers to their probe_vf values\n"
		 "\t\t(e.g. '0000:04:00.0-3,002b:1c:0b.a-13;12;11') could be given.\n"
		 "\t\tHexadecimal digits for the device function (e.g. 002b:1c:0b.a) and decimal for probe_vf value (e.g. 13 or 1;2;3).");

#define MLX4_FORCE_DMFS_IF_NO_NCSI_FS		(1U << 0)
#define MLX4_DMFS_ETH_ONLY			(1U << 1)
#define MLX4_DMFS_A0_STEERING			(1U << 2)
#define MLX4_DISABLE_DMFS_LOW_QP_NUM		(1U << 3)
#define MLX4_DMFS_PARAM_VALUES			((MLX4_DISABLE_DMFS_LOW_QP_NUM << 1) - 1)

int mlx4_log_num_mgm_entry_size = -(MLX4_DMFS_ETH_ONLY | MLX4_DISABLE_DMFS_LOW_QP_NUM);

module_param_named(log_num_mgm_entry_size,
			mlx4_log_num_mgm_entry_size, int, 0444);

MODULE_PARM_DESC(log_num_mgm_entry_size, "log mgm size, that defines the num"
					 " of qp per mcg, for example:"
					 " 10 gives 248.range: 7 <="
					 " log_num_mgm_entry_size <= 12 (default = -10).\n"
					 "\t\tTo activate one of device managed"
					 " flow steering modes, set to  non positive value (-x) and sets bits in x:\n"
					 "\t\t0: Force DMFS, even on expense of NCSI support\n"
					 "\t\t1: Disable IPoIB DMFS rules (if enabled performance might decrease. Can't be cleared if b3 is set)\n"
					 "\t\t2: Enable optimized steering (even if in limited L2 mode. Can't be set if b2 is cleared)\n"
					 "\t\t3: Disable DMFS if number of QPs per MCG is low\n");

static int high_rate_steer;
module_param(high_rate_steer, int, 0444);
MODULE_PARM_DESC(high_rate_steer, "Enable steering mode for higher packet rate"
				  " (default off)");

static int fast_drop;
module_param_named(fast_drop, fast_drop, int, 0444);
MODULE_PARM_DESC(fast_drop,
		 "Enable fast packet drop when no recieve WQEs are posted");

int mlx4_enable_64b_cqe_eqe = 1;
module_param_named(enable_64b_cqe_eqe, mlx4_enable_64b_cqe_eqe, int, 0644);
MODULE_PARM_DESC(enable_64b_cqe_eqe,
		 "Enable 64 byte CQEs/EQEs when the the FW supports this if non-zero (default: 1)");

#define HCA_GLOBAL_CAP_MASK            0

#define PF_CONTEXT_BEHAVIOUR_MASK	(MLX4_FUNC_CAP_64B_EQE_CQE |  \
					 MLX4_FUNC_CAP_EQE_CQE_STRIDE)

static char mlx4_version[] __devinitdata =
	DRV_NAME ": Mellanox ConnectX core driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

static int log_num_mac = 7;
module_param_named(log_num_mac, log_num_mac, int, 0444);
MODULE_PARM_DESC(log_num_mac, "Log2 max number of MACs per ETH port (1-7)");

static int log_num_vlan;
module_param_named(log_num_vlan, log_num_vlan, int, 0444);
MODULE_PARM_DESC(log_num_vlan,
	"(Obsolete) Log2 max number of VLANs per ETH port (0-7)");
/* Log2 max number of VLANs per ETH port (0-7) */
#define MLX4_LOG_NUM_VLANS 7

int log_mtts_per_seg = ilog2(1);
module_param_named(log_mtts_per_seg, log_mtts_per_seg, int, 0444);
MODULE_PARM_DESC(log_mtts_per_seg, "Log2 number of MTT entries per segment "
		 "(0-7) (default: 0)");

static struct param_data port_type_array = {
	.id		= PORT_TYPE_ARRAY,
	.dbdf2val = {
		.name		= "port_type_array param",
		.num_vals	= 2,
		.def_val	= {MLX4_PORT_TYPE_NONE, MLX4_PORT_TYPE_NONE},
		.range		= {MLX4_PORT_TYPE_IB, MLX4_PORT_TYPE_NA},
		.num_inval_vals = 1,
		.inval_val = {MLX4_PORT_TYPE_AUTO}
	}
};
module_param_string(port_type_array, port_type_array.dbdf2val.str,
		    sizeof(port_type_array.dbdf2val.str), 0444);
MODULE_PARM_DESC(port_type_array,
		 "Either pair of values (e.g. '1,2') to define uniform port1/port2 types configuration for all devices functions\n"
		 "\t\tor a string to map device function numbers to their pair of port types values (e.g. '0000:04:00.0-1;2,002b:1c:0b.a-1;1').\n"
		 "\t\tValid port types: 1-ib, 2-eth, 4-N/A\n"
		 "\t\tIn case that only one port is available use the N/A port type for port2 (e.g '1,4').");


struct mlx4_port_config {
	struct list_head list;
	enum mlx4_port_type port_type[MLX4_MAX_PORTS + 1];
	struct pci_dev *pdev;
};

#define MLX4_LOG_NUM_MTT 20
/* We limit to 30 as of a bit map issue which uses int and not uint.
     see mlx4_buddy_init -> bitmap_zero which gets int.
*/
#define MLX4_MAX_LOG_NUM_MTT 30
static struct mlx4_profile mod_param_profile = {
	.num_qp         = 19,
	.num_srq        = 16,
	.rdmarc_per_qp  = 4,
	.num_cq         = 16,
	.num_mcg        = 13,
	.num_mpt        = 19,
	.num_mtt_segs   = 0, /* max(20, 2*MTTs for host memory)) */
};

module_param_named(log_num_qp, mod_param_profile.num_qp, int, 0444);
MODULE_PARM_DESC(log_num_qp, "log maximum number of QPs per HCA (default: 19)");

module_param_named(log_num_srq, mod_param_profile.num_srq, int, 0444);
MODULE_PARM_DESC(log_num_srq, "log maximum number of SRQs per HCA "
		 "(default: 16)");

module_param_named(log_rdmarc_per_qp, mod_param_profile.rdmarc_per_qp, int,
		   0444);
MODULE_PARM_DESC(log_rdmarc_per_qp, "log number of RDMARC buffers per QP "
		 "(default: 4)");

module_param_named(log_num_cq, mod_param_profile.num_cq, int, 0444);
MODULE_PARM_DESC(log_num_cq, "log maximum number of CQs per HCA (default: 16)");

module_param_named(log_num_mcg, mod_param_profile.num_mcg, int, 0444);
MODULE_PARM_DESC(log_num_mcg, "log maximum number of multicast groups per HCA "
		 "(default: 13)");

module_param_named(log_num_mpt, mod_param_profile.num_mpt, int, 0444);
MODULE_PARM_DESC(log_num_mpt,
		 "log maximum number of memory protection table entries per "
		 "HCA (default: 19)");

module_param_named(log_num_mtt, mod_param_profile.num_mtt_segs, int, 0444);
MODULE_PARM_DESC(log_num_mtt,
		 "log maximum number of memory translation table segments per "
		 "HCA (default: max(20, 2*MTTs for register all of the host memory limited to 30))");

enum {
	MLX4_IF_STATE_BASIC,
	MLX4_IF_STATE_EXTENDED
};

static inline u64 dbdf_to_u64(int domain, int bus, int dev, int fn)
{
	return (domain << 20) | (bus << 12) | (dev << 4) | fn;
}

static inline void pr_bdf_err(const char *dbdf, const char *pname)
{
	pr_warn("mlx4_core: '%s' is not valid bdf in '%s'\n", dbdf, pname);
}

static inline void pr_val_err(const char *dbdf, const char *pname,
			      const char *val)
{
	pr_warn("mlx4_core: value '%s' of bdf '%s' in '%s' is not valid\n"
		, val, dbdf, pname);
}

static inline void pr_out_of_range_bdf(const char *dbdf, int val,
				       struct mlx4_dbdf2val_lst *dbdf2val)
{
	pr_warn("mlx4_core: value %d in bdf '%s' of '%s' is out of its valid range (%d,%d)\n"
		, val, dbdf, dbdf2val->name , dbdf2val->range.min,
		dbdf2val->range.max);
}

static inline void pr_out_of_range(struct mlx4_dbdf2val_lst *dbdf2val)
{
	pr_warn("mlx4_core: value of '%s' is out of its valid range (%d,%d)\n"
		, dbdf2val->name , dbdf2val->range.min, dbdf2val->range.max);
}

static inline int is_valid_value(int val, struct mlx4_dbdf2val_lst *v)
{
	int i;

	for (i = 0; i < v->num_inval_vals; i++) {
		if (val == v->inval_val[i])
			return 0;
	}
	return 1;
}

static inline void pr_invalid_value(int val, struct mlx4_dbdf2val_lst *dbdf2val)
{
	pr_warn("mlx4_core: value %d of '%s' is not allowed\n",
		val, dbdf2val->name);
}

static inline int is_in_range(int val, struct mlx4_range *r)
{
	return (val >= r->min && val <= r->max);
}

static int parse_array(struct param_data *pdata, char *p, long *vals, u32 n)
{
	u32 iter = 0;

	while (n != 0 && strlen(p)) {
		char *t = strchr(p, ',');
		int val_len = t - p;
		char sval[32];
		int ret;

		/* Try to parse as last element */
		if (!t && !kstrtol(p, 0, vals)) {
			if (!is_in_range(*vals, &pdata->dbdf2val.range)) {
				pr_out_of_range(&pdata->dbdf2val);
				return -INVALID_DATA;
			}
			if (!is_valid_value(*vals, &pdata->dbdf2val)) {
				pr_invalid_value(*vals, &pdata->dbdf2val);
				return -INVALID_DATA;
			}
			return ++iter;
		}

		if (!t || t == p || val_len > sizeof(sval))
			return -INVALID_STR;

		strncpy(sval, p, val_len);
		sval[val_len] = 0;

		ret = kstrtol(sval, 0, vals);

		if (ret == -EINVAL)
			return -INVALID_STR;
		if (ret || !is_in_range(*vals, &pdata->dbdf2val.range)) {
			pr_out_of_range(&pdata->dbdf2val);
			return -INVALID_DATA;
		}
		if (!is_valid_value(*vals, &pdata->dbdf2val)) {
			pr_invalid_value(*vals, &pdata->dbdf2val);
			return -INVALID_DATA;
		}

		++iter;
		++vals;
		p += val_len + 1;
		if (n > 0)
			n--;
	}

	return -INVALID_STR;
}

#define ARRAY_LEN(arr) (sizeof((arr))/sizeof((arr)[0]))
static int parse_mod_param(struct param_data *pdata)
{
	int i;
	int ret = 0;
	long port_array[ARRAY_LEN(pdata->dbdf2val.tbl[0].val)];
	char *p = pdata->dbdf2val.str;

	ret = parse_array(pdata, p, port_array,
			  pdata->dbdf2val.num_vals);
	if (ret > pdata->dbdf2val.num_vals || ret <= 0)
		return ret < 0 ? -ret : INVALID_STR;
	for (i = 0; i < ret; i++)
		pdata->dbdf2val.tbl[0].val[i] = port_array[i];
	pdata->dbdf2val.tbl[0].argc = i;
	return 0;
}

static int update_defaults(struct param_data *pdata)
{
	int ret;
	char *p = pdata->dbdf2val.str;

	if (!strlen(p) || strchr(p, ':') || strchr(p, '.') || strchr(p, ';'))
		return INVALID_STR;

	switch (pdata->id) {
	case ROCE_MODE:
	case PORT_TYPE_ARRAY:
	case NUM_VFS:
	case PROBE_VF:
		ret = parse_mod_param(pdata);
		if (ret)
			return ret;
		break;
	default:
		return INVALID_DATA;
	}
	pdata->dbdf2val.tbl[1].dbdf = MLX4_ENDOF_TBL;

	return VALID_DATA;
}

int mlx4_fill_dbdf2val_tbl(struct mlx4_dbdf2val_lst *dbdf2val_lst)
{
	int domain, bus, dev, fn;
	u64 dbdf;
	char *p, *t, *v;
	char tmp[32];
	char sbdf[32];
	char sep = ',';
	int j, k, str_size, i = 1;
	int prfx_size;

	p = dbdf2val_lst->str;

	for (j = 0; j < dbdf2val_lst->num_vals; j++)
		dbdf2val_lst->tbl[0].val[j] = dbdf2val_lst->def_val[j];
	dbdf2val_lst->tbl[0].argc = 0;
	dbdf2val_lst->tbl[1].dbdf = MLX4_ENDOF_TBL;

	str_size = strlen(dbdf2val_lst->str);

	if (str_size == 0)
		return 0;

	while (strlen(p)) {
		prfx_size = BDF_STR_SIZE;
		sbdf[prfx_size] = 0;
		strncpy(sbdf, p, prfx_size);
		domain = DEFAULT_DOMAIN;
		if (sscanf(sbdf, "%02x:%02x.%x-", &bus, &dev, &fn) != 3) {
			prfx_size = DBDF_STR_SIZE;
			sbdf[prfx_size] = 0;
			strncpy(sbdf, p, prfx_size);
			if (sscanf(sbdf, "%04x:%02x:%02x.%x-", &domain, &bus,
				   &dev, &fn) != 4) {
				pr_bdf_err(sbdf, dbdf2val_lst->name);
				goto err;
			}
			sprintf(tmp, "%04x:%02x:%02x.%x-", domain, bus, dev,
				fn);
		} else {
			sprintf(tmp, "%02x:%02x.%x-", bus, dev, fn);
		}

		if (strnicmp(sbdf, tmp, sizeof(tmp))) {
			pr_bdf_err(sbdf, dbdf2val_lst->name);
			goto err;
		}

		dbdf = dbdf_to_u64(domain, bus, dev, fn);

		for (j = 1; j < i; j++)
			if (dbdf2val_lst->tbl[j].dbdf == dbdf) {
				pr_warn("mlx4_core: in '%s', %s appears multiple times\n"
					, dbdf2val_lst->name, sbdf);
				goto err;
			}

		if (i >= MLX4_DEVS_TBL_SIZE) {
			pr_warn("mlx4_core: Too many devices in '%s'\n"
				, dbdf2val_lst->name);
			goto err;
		}

		p += prfx_size;
		t = strchr(p, sep);
		t = t ? t : p + strlen(p);
		if (p >= t) {
			pr_val_err(sbdf, dbdf2val_lst->name, "");
			goto err;
		}

		for (k = 0; k < dbdf2val_lst->num_vals; k++) {
			char sval[32];
			long int val;
			int ret, val_len;
			char vsep = ';';
			int last_occurence = 0;

			v = (k == dbdf2val_lst->num_vals - 1) ? t : strchr(p, vsep);
			if (NULL == v) {
				v = t;
				last_occurence = 1;
			}
			if (!v || v > t || v == p || (v - p) > sizeof(sval)) {
				pr_val_err(sbdf, dbdf2val_lst->name, p);
				goto err;
			}
			val_len = v - p;
			strncpy(sval, p, val_len);
			sval[val_len] = 0;

			ret = kstrtol(sval, 0, &val);
			if (ret) {
				if (strchr(p, vsep))
					pr_warn("mlx4_core: too many vals in bdf '%s' of '%s'\n"
						, sbdf, dbdf2val_lst->name);
				else
					pr_val_err(sbdf, dbdf2val_lst->name,
						   sval);
				goto err;
			}
			if (!is_in_range(val, &dbdf2val_lst->range)) {
				pr_out_of_range_bdf(sbdf, val, dbdf2val_lst);
				goto err;
			}

			dbdf2val_lst->tbl[i].val[k] = val;
			dbdf2val_lst->tbl[i].argc = k + 1;
			p = v;
			if (p[0] == vsep)
				p++;
			if (last_occurence)
				break;
		}

		dbdf2val_lst->tbl[i].dbdf = dbdf;
		if (strlen(p)) {
			if (p[0] != sep) {
				pr_warn("mlx4_core: expect separator '%c' before '%s' in '%s'\n"
					, sep, p, dbdf2val_lst->name);
				goto err;
			}
			p++;
		}
		i++;
		if (i < MLX4_DEVS_TBL_SIZE)
			dbdf2val_lst->tbl[i].dbdf = MLX4_ENDOF_TBL;
	}

	return 0;

err:
	dbdf2val_lst->tbl[1].dbdf = MLX4_ENDOF_TBL;
	pr_warn("mlx4_core: The value of '%s' is incorrect. The value is discarded!\n"
		, dbdf2val_lst->name);

	return -EINVAL;
}
EXPORT_SYMBOL(mlx4_fill_dbdf2val_tbl);

int mlx4_get_val(struct mlx4_dbdf2val *tbl, struct pci_dev *pdev, int idx,
		 int *val)
{
	u64 dbdf;
	int i = 1;

	*val = tbl[0].val[idx];
	if (!pdev)
		return -EINVAL;

	if (!pdev->bus) {
		pr_debug("mlx4_core: pci_dev without valid bus number\n");
		return -EINVAL;
	}

	dbdf = dbdf_to_u64(pci_domain_nr(pdev->bus), pdev->bus->number,
			   PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	while ((i < MLX4_DEVS_TBL_SIZE) && (tbl[i].dbdf != MLX4_ENDOF_TBL)) {
		if (tbl[i].dbdf == dbdf) {
			if (idx < tbl[i].argc) {
				*val = tbl[i].val[idx];
				return 0;
			} else {
				return -EINVAL;
			}
		}
		i++;
	}

	return 0;
}
EXPORT_SYMBOL(mlx4_get_val);


int mlx4_get_argc(struct mlx4_dbdf2val *tbl, struct pci_dev *pdev)
{
	u64 dbdf;
	int i = 1;

	if (!pdev)
		return -EINVAL;

	if (!pdev->bus) {
		pr_debug("mlx4_core: pci_dev without valid bus number\n");
		return -EINVAL;
	}

	dbdf = dbdf_to_u64(pci_domain_nr(pdev->bus), pdev->bus->number,
			   PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

	while ((i < MLX4_DEVS_TBL_SIZE) && (tbl[i].dbdf != MLX4_ENDOF_TBL)) {
		if (tbl[i].dbdf == dbdf)
			return tbl[i].argc;
		i++;
	}

	return tbl[0].argc;
}
EXPORT_SYMBOL(mlx4_get_argc);

static void process_mod_param_profile(struct mlx4_profile *profile)
{
	struct sysinfo si;

	profile->num_qp        = 1 << mod_param_profile.num_qp;
	profile->num_srq       = 1 << mod_param_profile.num_srq;
	profile->rdmarc_per_qp = 1 << mod_param_profile.rdmarc_per_qp;
	profile->num_cq	       = 1 << mod_param_profile.num_cq;
	profile->num_mcg       = 1 << mod_param_profile.num_mcg;
	profile->num_mpt       = 1 << mod_param_profile.num_mpt;
	/*
	 * We want to scale the number of MTTs with the size of the
	 * system memory, since it makes sense to register a lot of
	 * memory on a system with a lot of memory.  As a heuristic,
	 * make sure we have enough MTTs to register twice the system
	 * memory (with PAGE_SIZE entries).
	 *
	 * This number has to be a power of two and fit into 32 bits
	 * due to device limitations. We cap this at 2^30 as of bit map
	 * limitation to work with int instead of uint (mlx4_buddy_init -> bitmap_zero)
	 * That limits us to 4TB of memory registration per HCA with
	 * 4KB pages, which is probably OK for the next few months.
	 */
	if (mod_param_profile.num_mtt_segs)
		profile->num_mtt_segs = 1 << mod_param_profile.num_mtt_segs;
	else {
		si_meminfo(&si);
		profile->num_mtt_segs =
			roundup_pow_of_two(max_t(unsigned,
						1 << (MLX4_LOG_NUM_MTT - log_mtts_per_seg),
						min(1UL <<
						(MLX4_MAX_LOG_NUM_MTT -
						log_mtts_per_seg),
						(si.totalram << 1)
						>> log_mtts_per_seg)));
		/* set the actual value, so it will be reflected to the user
		   using the sysfs */
		mod_param_profile.num_mtt_segs = ilog2(profile->num_mtt_segs);
	}
}

#ifndef CONFIG_COMPAT_IS_PCI_PHYSFN
/* This function copied from linux/pci.h (kernel 2.6.35) */
static inline struct pci_dev *pci_physfn(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_IOV
       if (dev->is_virtfn)
               dev = dev->physfn;
#endif

       return dev;
}
#endif

int mlx4_check_port_params(struct mlx4_dev *dev,
			   enum mlx4_port_type *port_type)
{
	int i;

	for (i = 0; i < dev->caps.num_ports - 1; i++) {
		if (port_type[i] != port_type[i + 1]) {
			if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_DPDP)) {
				mlx4_err(dev, "Only same port types supported "
					 "on this HCA, aborting.\n");
				return -EINVAL;
			}
		}
	}

	for (i = 0; i < dev->caps.num_ports; i++) {
		if (!(port_type[i] & dev->caps.supported_type[i+1])) {
			mlx4_err(dev, "Requested port type for port %d is not "
				      "supported on this HCA\n", i + 1);
			return -EINVAL;
		}
	}
	return 0;
}

static void mlx4_set_port_mask(struct mlx4_dev *dev)
{
	int i;

	for (i = 1; i <= dev->caps.num_ports; ++i)
		dev->caps.port_mask[i] = dev->caps.port_type[i];
}

static int mlx4_query_func(struct mlx4_dev *dev, struct mlx4_dev_cap *dev_cap)
{
	int err = 0;
	struct mlx4_func func;

	if (dev->caps.num_sys_eqs) {
		err = mlx4_QUERY_FUNC(dev, &func, 0);
		if (err) {
			mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
			return err;
		}
		dev->caps.num_eqs = func.max_eq;
		dev_cap->max_eqs = func.max_eq;
		dev->caps.reserved_eqs = func.rsvd_eqs;
		dev_cap->reserved_eqs = func.rsvd_eqs;
		dev->caps.reserved_uars = func.rsvd_uars;
		dev_cap->reserved_uars = func.rsvd_uars;
	}
	return err;
}

static int _mlx4_dev_port(struct mlx4_dev *dev, int port,
			  struct mlx4_port_cap *port_cap)
{
	dev->caps.vl_cap[port]	    = port_cap->max_vl;
	dev->caps.ib_mtu_cap[port]	    = port_cap->ib_mtu;
	dev->phys_caps.gid_phys_table_len[port]  = port_cap->max_gids;
	dev->phys_caps.pkey_phys_table_len[port] = port_cap->max_pkeys;
	/* set gid and pkey table operating lengths by default
	 * to non-sriov values
	 */
	dev->caps.gid_table_len[port]  = port_cap->max_gids;
	dev->caps.pkey_table_len[port] = port_cap->max_pkeys;
	dev->caps.port_width_cap[port] = port_cap->max_port_width;
	dev->caps.eth_mtu_cap[port]    = port_cap->eth_mtu;
	dev->caps.def_mac[port]        = port_cap->def_mac;
	dev->caps.supported_type[port] = port_cap->supported_port_types;
	dev->caps.suggested_type[port] = port_cap->suggested_type;
	dev->caps.default_sense[port] = port_cap->default_sense;
	dev->caps.trans_type[port]	    = port_cap->trans_type;
	dev->caps.vendor_oui[port]     = port_cap->vendor_oui;
	dev->caps.wavelength[port]     = port_cap->wavelength;
	dev->caps.trans_code[port]     = port_cap->trans_code;

	return 0;
}

static int mlx4_dev_port(struct mlx4_dev *dev, int port,
			 struct mlx4_port_cap *port_cap)
{
	int err = 0;

	err = mlx4_QUERY_PORT(dev, port, port_cap);

	if (err)
		mlx4_err(dev, "QUERY_PORT command failed.\n");

	return err;
}

static void mlx4_enable_cqe_eqe_stride(struct mlx4_dev *dev)
{
	struct mlx4_caps *dev_cap = &dev->caps;

	/* FW not supporting or cancelled by user */
	if (!(dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_EQE_STRIDE) ||
	    !(dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_CQE_STRIDE))
		return;

	/* Must have 64B CQE_EQE enabled by FW to use bigger stride
	 * When FW has NCSI it may decide not to report 64B CQE/EQEs
	 */
	if (!(dev_cap->flags & MLX4_DEV_CAP_FLAG_64B_EQE) ||
	    !(dev_cap->flags & MLX4_DEV_CAP_FLAG_64B_CQE)) {
		dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_CQE_STRIDE;
		dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_EQE_STRIDE;
		return;
	}

	if (cache_line_size() == 128 || cache_line_size() == 256) {
		mlx4_dbg(dev, "Enabling CQE stride cacheLine supported\n");
		/* Changing the real CQE size to 32B, but nobody will know */
		dev_cap->flags &= ~MLX4_DEV_CAP_FLAG_64B_CQE;
		dev_cap->flags &= ~MLX4_DEV_CAP_FLAG_64B_EQE;

		if (mlx4_is_master(dev))
			dev_cap->function_caps |= MLX4_FUNC_CAP_EQE_CQE_STRIDE;
	} else {
		mlx4_dbg(dev, "Disabling CQE stride cacheLine unsupported\n");
		dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_CQE_STRIDE;
		dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_EQE_STRIDE;
	}
}

#define MLX4_A0_STEERING_TABLE_SIZE	256
static int mlx4_dev_cap(struct mlx4_dev *dev, struct mlx4_dev_cap *dev_cap)
{
	int err;
	int i;

	err = mlx4_QUERY_DEV_CAP(dev, dev_cap);
	if (err) {
		mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
		return err;
	}

	if (dev_cap->min_page_sz > PAGE_SIZE) {
		mlx4_err(dev, "HCA minimum page size of %d bigger than "
			 "kernel PAGE_SIZE of %ld, aborting.\n",
			 dev_cap->min_page_sz, PAGE_SIZE);
		return -ENODEV;
	}
	if (dev_cap->num_ports > MLX4_MAX_PORTS) {
		mlx4_err(dev, "HCA has %d ports, but we only support %d, "
			 "aborting.\n",
			 dev_cap->num_ports, MLX4_MAX_PORTS);
		return -ENODEV;
	}

	if (dev_cap->uar_size > pci_resource_len(dev->pdev, 2)) {
		mlx4_err(dev, "HCA reported UAR size of 0x%x bigger than "
			 "PCI resource 2 size of 0x%llx, aborting.\n",
			 dev_cap->uar_size,
			 (unsigned long long) pci_resource_len(dev->pdev, 2));
		return -ENODEV;
	}

	dev->caps.num_ports	     = dev_cap->num_ports;
	dev->caps.num_sys_eqs = dev_cap->num_sys_eqs;
	if (dev->caps.num_sys_eqs)
		dev->phys_caps.num_phys_eqs = dev->caps.num_sys_eqs;
	else
		dev->phys_caps.num_phys_eqs = MLX4_MAX_EQ_NUM;

	for (i = 1; i <= dev->caps.num_ports; ++i) {
		err = _mlx4_dev_port(dev, i, dev_cap->port_cap + i);
		if (err) {
			mlx4_err(dev, "QUERY_PORT command failed, aborting\n");
			return err;
		}
	}

	dev->caps.uar_page_size	     = PAGE_SIZE;
	dev->caps.num_uars	     = dev_cap->uar_size / PAGE_SIZE;
	dev->caps.local_ca_ack_delay = dev_cap->local_ca_ack_delay;
	dev->caps.bf_reg_size	     = dev_cap->bf_reg_size;
	dev->caps.bf_regs_per_page   = dev_cap->bf_regs_per_page;
	dev->caps.max_sq_sg	     = dev_cap->max_sq_sg;
	dev->caps.max_rq_sg	     = dev_cap->max_rq_sg;
	dev->caps.max_wqes	     = dev_cap->max_qp_sz;
	dev->caps.max_qp_init_rdma   = dev_cap->max_requester_per_qp;
	dev->caps.max_srq_wqes	     = dev_cap->max_srq_sz;
	dev->caps.max_srq_sge	     = dev_cap->max_rq_sg - 1;
	dev->caps.reserved_srqs	     = dev_cap->reserved_srqs;
	dev->caps.max_sq_desc_sz     = dev_cap->max_sq_desc_sz;
	dev->caps.max_rq_desc_sz     = dev_cap->max_rq_desc_sz;
	/*
	 * Subtract 1 from the limit because we need to allocate a
	 * spare CQE to enable resizing the CQ
	 */
	dev->caps.max_cqes	     = dev_cap->max_cq_sz - 1;
	dev->caps.reserved_cqs	     = dev_cap->reserved_cqs;
	dev->caps.reserved_eqs	     = dev_cap->reserved_eqs;
	dev->caps.reserved_mtts      = dev_cap->reserved_mtts;
	dev->caps.reserved_mrws	     = dev_cap->reserved_mrws;

	/* The first 128 UARs are used for EQ doorbells */
	dev->caps.reserved_uars	     = max_t(int, 128, dev_cap->reserved_uars);
	dev->caps.reserved_pds	     = dev_cap->reserved_pds;
	dev->caps.reserved_xrcds     = (dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC) ?
					dev_cap->reserved_xrcds : 0;
	dev->caps.max_xrcds          = (dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC) ?
					dev_cap->max_xrcds : 0;
	dev->caps.mtt_entry_sz       = dev_cap->mtt_entry_sz;

	dev->caps.max_msg_sz         = dev_cap->max_msg_sz;
	dev->caps.page_size_cap	     = ~(u32) (dev_cap->min_page_sz - 1);
	dev->caps.flags		     = dev_cap->flags;
	dev->caps.flags2	     = dev_cap->flags2;
	dev->caps.bmme_flags	     = dev_cap->bmme_flags;
	dev->caps.reserved_lkey	     = dev_cap->reserved_lkey;
	dev->caps.stat_rate_support  = dev_cap->stat_rate_support;
	dev->caps.max_gso_sz	     = dev_cap->max_gso_sz;
	dev->caps.max_rss_tbl_sz     = dev_cap->max_rss_tbl_sz;
	dev->caps.cq_overrun	     = dev_cap->cq_overrun;

	/* Sense port always allowed on supported devices for ConnectX-1 and -2 */
	if (mlx4_priv(dev)->pci_dev_data & MLX4_PCI_DEV_FORCE_SENSE_PORT)
		dev->caps.flags |= MLX4_DEV_CAP_FLAG_SENSE_SUPPORT;
	/* Don't do sense port on multifunction devices (for now at least) */
	if (mlx4_is_mfunc(dev))
		dev->caps.flags &= ~MLX4_DEV_CAP_FLAG_SENSE_SUPPORT;

	dev->caps.log_num_macs  = log_num_mac;
	dev->caps.log_num_vlans = MLX4_LOG_NUM_VLANS;
	dev->caps.ecn_qcn_ver = dev_cap->ecn_qcn_ver;

	dev->caps.fast_drop	= fast_drop ?
				  !!(dev->caps.flags & MLX4_DEV_CAP_FLAG_FAST_DROP) :
				  0;

	for (i = 1; i <= dev->caps.num_ports; ++i) {
		dev->caps.port_type[i] = MLX4_PORT_TYPE_NONE;
		if (dev->caps.supported_type[i]) {
			/* if only ETH is supported - assign ETH */
			if (dev->caps.supported_type[i] == MLX4_PORT_TYPE_ETH)
				dev->caps.port_type[i] = MLX4_PORT_TYPE_ETH;
			/* if only IB is supported, assign IB */
			else if (dev->caps.supported_type[i] ==
				 MLX4_PORT_TYPE_IB)
				dev->caps.port_type[i] = MLX4_PORT_TYPE_IB;
			else {
				/*
				 * if IB and ETH are supported, we set the port
				 * type according to user selection of port type;
				 * if there is no user selection, take the FW hint
				 */
				int pta;
				mlx4_get_val(port_type_array.dbdf2val.tbl,
					     pci_physfn(dev->pdev), i - 1,
					     &pta);
				if (pta == MLX4_PORT_TYPE_NONE) {
					dev->caps.port_type[i] = dev->caps.suggested_type[i] ?
						MLX4_PORT_TYPE_ETH : MLX4_PORT_TYPE_IB;
				} else if (pta == MLX4_PORT_TYPE_NA) {
					mlx4_err(dev, "Port %d is valid port. "
						 "It is not allowed to configure its type to N/A(%d)\n",
						 i, MLX4_PORT_TYPE_NA);
					return -EINVAL;
				} else {
					dev->caps.port_type[i] = pta;
				}
			}
		}
		/*
		 * Link sensing is allowed on the port if 3 conditions are true:
		 * 1. Both protocols are supported on the port.
		 * 2. Different types are supported on the port
		 * 3. FW declared that it supports link sensing
		 */
		mlx4_priv(dev)->sense.sense_allowed[i] =
			((dev->caps.supported_type[i] == MLX4_PORT_TYPE_AUTO) &&
			 (dev->caps.flags & MLX4_DEV_CAP_FLAG_DPDP) &&
			 (dev->caps.flags & MLX4_DEV_CAP_FLAG_SENSE_SUPPORT));

		/*
		 * If "default_sense" bit is set, we move the port to "AUTO" mode
		 * and perform sense_port FW command to try and set the correct
		 * port type from beginning
		 */
		if (mlx4_priv(dev)->sense.sense_allowed[i] && dev->caps.default_sense[i]) {
			enum mlx4_port_type sensed_port = MLX4_PORT_TYPE_NONE;
			dev->caps.possible_type[i] = MLX4_PORT_TYPE_AUTO;
			mlx4_SENSE_PORT(dev, i, &sensed_port);
			if (sensed_port != MLX4_PORT_TYPE_NONE)
				dev->caps.port_type[i] = sensed_port;
		} else {
			dev->caps.possible_type[i] = dev->caps.port_type[i];
		}

		if (dev->caps.log_num_macs > dev_cap->port_cap[i].log_max_macs) {
			dev->caps.log_num_macs = dev_cap->port_cap[i].log_max_macs;
			mlx4_warn(dev, "Requested number of MACs is too much "
				  "for port %d, reducing to %d.\n",
				  i, 1 << dev->caps.log_num_macs);
		}
		if (dev->caps.log_num_vlans > dev_cap->port_cap[i].log_max_vlans) {
			dev->caps.log_num_vlans = dev_cap->port_cap[i].log_max_vlans;
			mlx4_warn(dev, "Requested number of VLANs is too much "
				  "for port %d, reducing to %d.\n",
				  i, 1 << dev->caps.log_num_vlans);
		}
	}

	dev->caps.max_basic_counters = dev_cap->max_basic_counters;
	dev->caps.max_extended_counters = dev_cap->max_extended_counters;
	/* support extended counters if available */
	if (dev->caps.flags & MLX4_DEV_CAP_FLAG_COUNTERS_EXT)
		dev->caps.max_counters = dev->caps.max_extended_counters;
	else
		dev->caps.max_counters = dev->caps.max_basic_counters;

	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW] = dev_cap->reserved_qps;
	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_ETH_ADDR] =
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_ADDR] =
		(1 << dev->caps.log_num_macs) *
		(1 << dev->caps.log_num_vlans) *
		dev->caps.num_ports;
	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_EXCH] = MLX4_NUM_FEXCH;

	if (dev_cap->dmfs_high_rate_qpn_base > 0 &&
	    dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_FS_EN)
		dev->caps.dmfs_high_rate_qpn_base = dev_cap->dmfs_high_rate_qpn_base;
	else
		dev->caps.dmfs_high_rate_qpn_base =
			dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW];

	if (dev_cap->dmfs_high_rate_qpn_range > 0 &&
	    dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_FS_EN) {
		dev->caps.dmfs_high_rate_qpn_range = dev_cap->dmfs_high_rate_qpn_range;
		dev->caps.dmfs_high_steer_mode = MLX4_STEERING_DMFS_A0_DEFAULT;
	} else {
		dev->caps.dmfs_high_steer_mode = MLX4_STEERING_DMFS_A0_NOT_SUPPORTED;
		dev->caps.dmfs_high_rate_qpn_base =
			dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW];
		dev->caps.dmfs_high_rate_qpn_range = MLX4_A0_STEERING_TABLE_SIZE;
	}

	dev->caps.reserved_qps_cnt[MLX4_QP_REGION_RSS_RAW_ETH] =
		dev->caps.dmfs_high_rate_qpn_range;

	dev->caps.reserved_qps = dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_ETH_ADDR] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_ADDR] +
		dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FC_EXCH];

	dev->caps.sync_qp = dev_cap->sync_qp;
	if (dev->pdev->device == 0x1003 || dev->caps.cq_overrun)
		dev->caps.cq_flags |= MLX4_DEV_CAP_CQ_FLAG_IO;

	dev->caps.sqp_demux = (mlx4_is_master(dev)) ? MLX4_MAX_NUM_SLAVES : 0;

	if (!mlx4_enable_64b_cqe_eqe && !mlx4_is_slave(dev)) {
		if (dev_cap->flags &
		    (MLX4_DEV_CAP_FLAG_64B_CQE | MLX4_DEV_CAP_FLAG_64B_EQE)) {
			mlx4_warn(dev, "64B EQEs/CQEs supported by the device but not enabled\n");
			dev->caps.flags &= ~MLX4_DEV_CAP_FLAG_64B_CQE;
			dev->caps.flags &= ~MLX4_DEV_CAP_FLAG_64B_EQE;
		}

		if (dev_cap->flags2 &
		    (MLX4_DEV_CAP_FLAG2_CQE_STRIDE |
		     MLX4_DEV_CAP_FLAG2_EQE_STRIDE)) {
			mlx4_warn(dev, "Disabling EQE/CQE stride as well\n");
			dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_CQE_STRIDE;
			dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_EQE_STRIDE;
		}
	}

	if ((dev->caps.flags &
	    (MLX4_DEV_CAP_FLAG_64B_CQE | MLX4_DEV_CAP_FLAG_64B_EQE)) &&
	    mlx4_is_master(dev))
		dev->caps.function_caps |= MLX4_FUNC_CAP_64B_EQE_CQE;

	if (!mlx4_is_slave(dev))
		mlx4_enable_cqe_eqe_stride(dev);

	dev->caps.mad_demux = dev_cap->mad_demux;

	if (!mlx4_is_slave(dev)) {
		for (i = 0; i < dev->caps.num_ports; ++i)
			dev->caps.def_counter_index[i] = i << 1;
	}

	return 0;
}
/*The function checks if there are live vf, return the num of them*/
static int mlx4_how_many_lives_vf(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_slave_state *s_state;
	int i;
	int ret = 0;

	for (i = 1/*the ppf is 0*/; i < dev->num_slaves; ++i) {
		s_state = &priv->mfunc.master.slave_state[i];
		if (s_state->active && s_state->last_cmd !=
		    MLX4_COMM_CMD_RESET) {
			mlx4_warn(dev, "%s: slave: %d is still active\n",
				  __func__, i);
			ret++;
		}
	}
	return ret;
}

int mlx4_get_parav_qkey(struct mlx4_dev *dev, u32 qpn, u32 *qkey)
{
	u32 qk = MLX4_RESERVED_QKEY_BASE;

	if (qpn >= dev->phys_caps.base_tunnel_sqpn + 8 * MLX4_MFUNC_MAX ||
	    qpn < dev->phys_caps.base_proxy_sqpn)
		return -EINVAL;

	if (qpn >= dev->phys_caps.base_tunnel_sqpn)
		/* tunnel qp */
		qk += qpn - dev->phys_caps.base_tunnel_sqpn;
	else
		qk += qpn - dev->phys_caps.base_proxy_sqpn;
	*qkey = qk;
	return 0;
}
EXPORT_SYMBOL(mlx4_get_parav_qkey);

void mlx4_sync_pkey_table(struct mlx4_dev *dev, int slave, int port, int i, int val)
{
	struct mlx4_priv *priv = container_of(dev, struct mlx4_priv, dev);

	if (!mlx4_is_master(dev))
		return;

	priv->virt2phys_pkey[slave][port - 1][i] = val;
}
EXPORT_SYMBOL(mlx4_sync_pkey_table);

void mlx4_put_slave_node_guid(struct mlx4_dev *dev, int slave, __be64 guid)
{
	struct mlx4_priv *priv = container_of(dev, struct mlx4_priv, dev);

	if (!mlx4_is_master(dev))
		return;

	priv->slave_node_guids[slave] = guid;
}
EXPORT_SYMBOL(mlx4_put_slave_node_guid);

__be64 mlx4_get_slave_node_guid(struct mlx4_dev *dev, int slave)
{
	struct mlx4_priv *priv = container_of(dev, struct mlx4_priv, dev);

	if (!mlx4_is_master(dev))
		return 0;

	return priv->slave_node_guids[slave];
}
EXPORT_SYMBOL(mlx4_get_slave_node_guid);

int mlx4_is_slave_active(struct mlx4_dev *dev, int slave)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_slave_state *s_slave;

	if (!mlx4_is_master(dev))
		return 0;

	s_slave = &priv->mfunc.master.slave_state[slave];
	return !!s_slave->active;
}
EXPORT_SYMBOL(mlx4_is_slave_active);

static void slave_adjust_steering_mode(struct mlx4_dev *dev,
				       struct mlx4_dev_cap *dev_cap,
				       struct mlx4_init_hca_param *hca_param)
{
	dev->caps.steering_mode = hca_param->steering_mode;
	if (dev->caps.steering_mode == MLX4_STEERING_MODE_DEVICE_MANAGED) {
		/* Always indicate the DMFS is forces (as we passed NCSI check if it
		 * was required. IPoIB DMFS is not supported for slaves.
		 */
		dev->caps.num_qp_per_mgm = dev_cap->fs_max_num_qp_per_entry;
		dev->caps.steering_attr |= MLX4_STEERING_ATTR_DMFS_EN;
		dev->caps.dmfs_high_steer_mode = hca_param->dmfs_high_steer_mode;
	} else {
		dev->caps.num_qp_per_mgm =
			4 * ((1 << hca_param->log_mc_entry_sz)/16 - 2);
	}

	mlx4_dbg(dev, "Steering mode is: %s\n",
		 mlx4_steering_mode_str(dev->caps.steering_mode));
}

const char *mlx4_roce_mode_str(int roce_mode)
{
	switch (roce_mode) {
	case MLX4_ROCE_MODE_NONE:
		return "RoCE not supported";
	case MLX4_ROCE_MODE_1:
		return "RoCE V1";
	case MLX4_ROCE_MODE_1_5:
		return "RoCE V1.5";
	case MLX4_ROCE_MODE_2:
		return "RoCE V2";
	default:
		return "unknown";
	}
}
EXPORT_SYMBOL_GPL(mlx4_roce_mode_str);

static void choose_roce_mode(struct mlx4_dev *dev,
			     struct mlx4_dev_cap *dev_cap)
{
	int req_roce_mode;
	int done_choose = 0;

	mlx4_get_val(roce_mode.dbdf2val.tbl, pci_physfn(dev->pdev), 0, &req_roce_mode);
	dev->caps.roce_mode = req_roce_mode;
	do {
		switch (dev->caps.roce_mode) {
		case MLX4_ROCE_MODE_1:
			if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_IBOE))
				dev->caps.roce_mode = MLX4_ROCE_MODE_1_5;
			else
				done_choose = 1;
			break;
		case MLX4_ROCE_MODE_1_5:
			if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_R_ROCE))
				dev->caps.roce_mode = MLX4_ROCE_MODE_2;
			else
				done_choose = 1;
			break;
		case MLX4_ROCE_MODE_2:
			if (!(dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ROCEV2))
				dev->caps.roce_mode = MLX4_ROCE_MODE_1;
			else
				done_choose = 1;
			break;
		default:
			dev->caps.roce_mode = MLX4_ROCE_MODE_NONE;
			done_choose = 1;
		}
		if (dev->caps.roce_mode == req_roce_mode && !done_choose) {
			dev->caps.roce_mode = MLX4_ROCE_MODE_NONE;
			break;
		}
	} while (!done_choose);

	if (req_roce_mode != dev->caps.roce_mode) {
		if (dev->caps.roce_mode == MLX4_ROCE_MODE_NONE) {
			mlx4_err(dev, "RoCE is not supported or requested mode %s is illegal\n",
				 mlx4_roce_mode_str(req_roce_mode));
		} else {
			mlx4_warn(dev, "Requested RoCE mode %s is not supported. RoCE will work in mode %s\n",
				  mlx4_roce_mode_str(req_roce_mode),
				  mlx4_roce_mode_str(dev->caps.roce_mode));
		}
	}
}

static int mlx4_slave_cap(struct mlx4_dev *dev)
{
	int			   err;
	u32			   page_size;
	struct mlx4_dev_cap	   *dev_cap = NULL;
	struct mlx4_func_cap	   *func_cap = NULL;
	struct mlx4_init_hca_param *hca_param = NULL;
	int			   i;

	hca_param = kzalloc(sizeof(*hca_param), GFP_KERNEL);
	func_cap = kzalloc(sizeof(*func_cap), GFP_KERNEL);
	dev_cap = kzalloc(sizeof(*dev_cap), GFP_KERNEL);
	if (!hca_param || !func_cap || !dev_cap) {
		mlx4_err(dev, "Failed to allocate memory for slave_cap\n");
		err = -ENOMEM;
		goto free_mem;
	}
	err = mlx4_QUERY_HCA(dev, hca_param);
	if (err) {
		mlx4_err(dev, "QUERY_HCA command failed, aborting.\n");
		goto free_mem;
	}

	/*fail if the hca has an unknown capability */
	if ((hca_param->global_caps | HCA_GLOBAL_CAP_MASK) !=
	    HCA_GLOBAL_CAP_MASK) {
		mlx4_err(dev, "Unknown hca global capabilities\n");
		err = -ENOSYS;
		goto free_mem;
	}

	dev->caps.hca_core_clock = hca_param->hca_core_clock;

	dev->caps.max_qp_dest_rdma = 1 << hca_param->log_rd_per_qp;
	err = mlx4_dev_cap(dev, dev_cap);
	if (err) {
		mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
		goto free_mem;
	}

	err = mlx4_QUERY_FW(dev);
	if (err)
		mlx4_err(dev, "QUERY_FW command failed: could not get FW version.\n");

	if (!hca_param->mw_enable) {
		dev->caps.flags      &= ~MLX4_DEV_CAP_FLAG_MEM_WINDOW;
		dev->caps.bmme_flags &= ~MLX4_BMME_FLAG_TYPE_2_WIN;
	}

	page_size = ~dev->caps.page_size_cap + 1;
	mlx4_warn(dev, "HCA minimum page size:%d\n", page_size);
	if (page_size > PAGE_SIZE) {
		mlx4_err(dev, "HCA minimum page size of %d bigger than "
			 "kernel PAGE_SIZE of %ld, aborting.\n",
			 page_size, PAGE_SIZE);
		err = -ENODEV;
		goto free_mem;
	}

	/* slave gets uar page size from QUERY_HCA fw command */
	dev->caps.uar_page_size = 1 << (hca_param->uar_page_sz + 12);

	/* TODO: relax this assumption */
	if (dev->caps.uar_page_size != PAGE_SIZE) {
		mlx4_err(dev, "UAR size:%d != kernel PAGE_SIZE of %ld\n",
			 dev->caps.uar_page_size, PAGE_SIZE);
		err = -ENODEV;
		goto free_mem;
	}

	err = mlx4_QUERY_FUNC_CAP(dev, 0, func_cap);
	if (err) {
		mlx4_err(dev, "QUERY_FUNC_CAP general command failed, aborting (%d).\n",
			  err);
		goto free_mem;
	}

	if ((func_cap->pf_context_behaviour | PF_CONTEXT_BEHAVIOUR_MASK) !=
	    PF_CONTEXT_BEHAVIOUR_MASK) {
		mlx4_err(dev, "Unknown pf context behaviour\n");
		err = -ENOSYS;
		goto free_mem;
	}

	dev->caps.num_ports		= func_cap->num_ports;
	dev->quotas.qp			= func_cap->qp_quota;
	dev->quotas.srq			= func_cap->srq_quota;
	dev->quotas.cq			= func_cap->cq_quota;
	dev->quotas.mpt			= func_cap->mpt_quota;
	dev->quotas.mtt			= func_cap->mtt_quota;
	dev->caps.num_qps		= 1 << hca_param->log_num_qps;
	dev->caps.num_srqs		= 1 << hca_param->log_num_srqs;
	dev->caps.num_cqs		= 1 << hca_param->log_num_cqs;
	dev->caps.num_mpts		= 1 << hca_param->log_mpt_sz;
	dev->caps.num_eqs		= func_cap->max_eq;
	dev->caps.reserved_eqs		= func_cap->reserved_eq;
	dev->caps.reserved_lkey		= func_cap->reserved_lkey;
	dev->caps.num_pds               = MLX4_NUM_PDS;
	dev->caps.num_mgms              = 0;
	dev->caps.num_amgms             = 0;

	if (dev->caps.num_ports > MLX4_MAX_PORTS) {
		mlx4_err(dev, "HCA has %d ports, but we only support %d, "
			 "aborting.\n", dev->caps.num_ports, MLX4_MAX_PORTS);
		err = -ENODEV;
		goto free_mem;
	}

	dev->caps.qp0_qkey = kcalloc(dev->caps.num_ports, sizeof(u32), GFP_KERNEL);
	dev->caps.qp0_tunnel = kcalloc(dev->caps.num_ports, sizeof (u32), GFP_KERNEL);
	dev->caps.qp0_proxy = kcalloc(dev->caps.num_ports, sizeof (u32), GFP_KERNEL);
	dev->caps.qp1_tunnel = kcalloc(dev->caps.num_ports, sizeof (u32), GFP_KERNEL);
	dev->caps.qp1_proxy = kcalloc(dev->caps.num_ports, sizeof (u32), GFP_KERNEL);

	if (!dev->caps.qp0_tunnel || !dev->caps.qp0_proxy ||
	    !dev->caps.qp1_tunnel || !dev->caps.qp1_proxy ||
	    !dev->caps.qp0_qkey) {
		err = -ENOMEM;
		goto err_mem;
	}

	for (i = 1; i <= dev->caps.num_ports; ++i) {
		err = mlx4_QUERY_FUNC_CAP(dev, (u8) i, func_cap);
		if (err) {
			mlx4_err(dev, "QUERY_FUNC_CAP port command failed for"
				 " port %d, aborting (%d).\n", i, err);
			goto err_mem;
		}
		dev->caps.force_vlan[i - 1] = func_cap->fvl;
		dev->caps.qp0_qkey[i - 1] = func_cap->qp0_qkey;
		dev->caps.qp0_tunnel[i - 1] = func_cap->qp0_tunnel_qpn;
		dev->caps.qp0_proxy[i - 1] = func_cap->qp0_proxy_qpn;
		dev->caps.qp1_tunnel[i - 1] = func_cap->qp1_tunnel_qpn;
		dev->caps.qp1_proxy[i - 1] = func_cap->qp1_proxy_qpn;
		dev->caps.def_counter_index[i - 1] = func_cap->def_counter_index;

		dev->caps.port_mask[i] = dev->caps.port_type[i];
		err = mlx4_get_slave_pkey_gid_tbl_len(dev, i,
						      &dev->caps.gid_table_len[i],
						      &dev->caps.pkey_table_len[i]);
		if (err)
			goto err_mem;
	}

	if (dev->caps.uar_page_size * (dev->caps.num_uars -
				       dev->caps.reserved_uars) >
				       pci_resource_len(dev->pdev, 2)) {
		mlx4_err(dev, "HCA reported UAR region size of 0x%x bigger than "
			 "PCI resource 2 size of 0x%llx, aborting.\n",
			 dev->caps.uar_page_size * dev->caps.num_uars,
			 (unsigned long long) pci_resource_len(dev->pdev, 2));
		err = -ENOMEM;
		goto err_mem;
	}

	if (hca_param->dev_cap_enabled & MLX4_DEV_CAP_64B_EQE_ENABLED) {
		dev->caps.eqe_size   = 64;
		dev->caps.eqe_factor = 1;
	} else {
		dev->caps.eqe_size   = 32;
		dev->caps.eqe_factor = 0;
	}

	if (hca_param->dev_cap_enabled & MLX4_DEV_CAP_64B_CQE_ENABLED) {
		dev->caps.cqe_size   = 64;
		dev->caps.userspace_caps |= MLX4_USER_DEV_CAP_LARGE_CQE;
	} else {
		dev->caps.cqe_size   = 32;
	}

	if (hca_param->dev_cap_enabled & MLX4_DEV_CAP_EQE_STRIDE_ENABLED)
		dev->caps.eqe_size = hca_param->eqe_size;

	if (hca_param->dev_cap_enabled & MLX4_DEV_CAP_CQE_STRIDE_ENABLED) {
		dev->caps.cqe_size = hca_param->cqe_size;

		/* User still need to know when CQE > 32B */
		dev->caps.userspace_caps |= MLX4_USER_DEV_CAP_LARGE_CQE;
	}

	dev->caps.flags2 &= ~MLX4_DEV_CAP_FLAG2_TS;
	mlx4_warn(dev, "Timestamping is not supported in slave mode.\n");

	slave_adjust_steering_mode(dev, dev_cap, hca_param);
	choose_roce_mode(dev, dev_cap);

	goto free_mem;

err_mem:
	kfree(dev->caps.qp0_qkey);
	kfree(dev->caps.qp0_tunnel);
	kfree(dev->caps.qp0_proxy);
	kfree(dev->caps.qp1_tunnel);
	kfree(dev->caps.qp1_proxy);
	dev->caps.qp0_qkey = NULL;
	dev->caps.qp0_tunnel = NULL;
	dev->caps.qp0_proxy = NULL;
	dev->caps.qp1_tunnel = NULL;
	dev->caps.qp1_proxy = NULL;

free_mem:
	kfree(hca_param);
	kfree(func_cap);
	kfree(dev_cap);
	return err;
}

static void mlx4_request_modules(struct mlx4_dev *dev)
{
	int port;
	int has_ib_port = false;
	int has_eth_port = false;
#define EN_DRV_NAME	"mlx4_en"
#define IB_DRV_NAME	"mlx4_ib"

	for (port = 1; port <= dev->caps.num_ports; port++) {
		if (dev->caps.port_type[port] == MLX4_PORT_TYPE_IB)
			has_ib_port = true;
		else if (dev->caps.port_type[port] == MLX4_PORT_TYPE_ETH)
			has_eth_port = true;
	}

	if (has_eth_port)
		request_module_nowait(EN_DRV_NAME);
	if (has_ib_port || (dev->caps.flags & MLX4_DEV_CAP_FLAG_IBOE))
		request_module_nowait(IB_DRV_NAME);
}

/*
 * Change the port configuration of the device.
 * Every user of this function must hold the port mutex.
 */
int mlx4_change_port_types(struct mlx4_dev *dev,
			   enum mlx4_port_type *port_types)
{
	int err = 0;
	int change = 0;
	int port;

	for (port = 0; port <  dev->caps.num_ports; port++) {
		/* Change the port type only if the new type is different
		 * from the current, and not set to Auto */
		if (port_types[port] != dev->caps.port_type[port + 1])
			change = 1;
	}
	if (change) {
		mlx4_unregister_device(dev);
		for (port = 1; port <= dev->caps.num_ports; port++) {
			mlx4_CLOSE_PORT(dev, port);
			dev->caps.port_type[port] = port_types[port - 1];
			err = mlx4_SET_PORT(dev, port, -1);
			if (err) {
				mlx4_err(dev, "Failed to set port %d, "
					      "aborting\n", port);
				goto out;
			}
		}
		mlx4_set_port_mask(dev);
		err = mlx4_register_device(dev);
		if (err) {
			mlx4_err(dev, "Failed to register device\n");
			goto out;
		}
		mlx4_request_modules(dev);
	}

out:
	return err;
}

static ssize_t show_port_type(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_attr);
	struct mlx4_dev *mdev = info->dev;
	char type[8];

	sprintf(type, "%s",
		(mdev->caps.port_type[info->port] == MLX4_PORT_TYPE_IB) ?
		"ib" : "eth");
	if (mdev->caps.possible_type[info->port] == MLX4_PORT_TYPE_AUTO)
		sprintf(buf, "auto (%s)\n", type);
	else
		sprintf(buf, "%s\n", type);

	return strlen(buf);
}

static ssize_t set_port_type(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_attr);
	struct mlx4_dev *mdev = info->dev;
	struct mlx4_priv *priv = mlx4_priv(mdev);
	enum mlx4_port_type types[MLX4_MAX_PORTS];
	enum mlx4_port_type new_types[MLX4_MAX_PORTS];
	int i;
	int err = 0;

	if (!strcmp(buf, "ib\n"))
		info->tmp_type = MLX4_PORT_TYPE_IB;
	else if (!strcmp(buf, "eth\n"))
		info->tmp_type = MLX4_PORT_TYPE_ETH;
	else if (!strcmp(buf, "auto\n"))
		info->tmp_type = MLX4_PORT_TYPE_AUTO;
	else {
		mlx4_err(mdev, "%s is not supported port type\n", buf);
		return -EINVAL;
	}

	if ((info->tmp_type & mdev->caps.supported_type[info->port]) !=
	    info->tmp_type) {
		mlx4_err(mdev, "Requested port type for port %d is not supported on this HCA\n",
			 info->port);
		return -EINVAL;
	}

	mlx4_stop_sense(mdev);
	mutex_lock(&priv->port_mutex);
	/* Possible type is always the one that was delivered */
	mdev->caps.possible_type[info->port] = info->tmp_type;

	for (i = 0; i < mdev->caps.num_ports; i++) {
		types[i] = priv->port[i+1].tmp_type ? priv->port[i+1].tmp_type :
					mdev->caps.possible_type[i+1];
		if (types[i] == MLX4_PORT_TYPE_AUTO)
			types[i] = mdev->caps.port_type[i+1];
	}

	if (!(mdev->caps.flags & MLX4_DEV_CAP_FLAG_DPDP) &&
	    !(mdev->caps.flags & MLX4_DEV_CAP_FLAG_SENSE_SUPPORT)) {
		for (i = 1; i <= mdev->caps.num_ports; i++) {
			if (mdev->caps.possible_type[i] == MLX4_PORT_TYPE_AUTO) {
				mdev->caps.possible_type[i] = mdev->caps.port_type[i];
				err = -EINVAL;
			}
		}
	}
	if (err) {
		mlx4_err(mdev, "Auto sensing is not supported on this HCA. "
			       "Set only 'eth' or 'ib' for both ports "
			       "(should be the same)\n");
		goto out;
	}

	mlx4_do_sense_ports(mdev, new_types, types);

	err = mlx4_check_port_params(mdev, new_types);
	if (err)
		goto out;

	/* We are about to apply the changes after the configuration
	 * was verified, no need to remember the temporary types
	 * any more */
	for (i = 0; i < mdev->caps.num_ports; i++)
		priv->port[i + 1].tmp_type = 0;

	err = mlx4_change_port_types(mdev, new_types);

out:
	mlx4_start_sense(mdev);
	mutex_unlock(&priv->port_mutex);
	return err ? err : count;
}

enum ibta_mtu {
	IB_MTU_256  = 1,
	IB_MTU_512  = 2,
	IB_MTU_1024 = 3,
	IB_MTU_2048 = 4,
	IB_MTU_4096 = 5
};

static inline int int_to_ibta_mtu(int mtu)
{
	switch (mtu) {
	case 256:  return IB_MTU_256;
	case 512:  return IB_MTU_512;
	case 1024: return IB_MTU_1024;
	case 2048: return IB_MTU_2048;
	case 4096: return IB_MTU_4096;
	default: return -1;
	}
}

static inline int ibta_mtu_to_int(enum ibta_mtu mtu)
{
	switch (mtu) {
	case IB_MTU_256:  return  256;
	case IB_MTU_512:  return  512;
	case IB_MTU_1024: return 1024;
	case IB_MTU_2048: return 2048;
	case IB_MTU_4096: return 4096;
	default: return -1;
	}
}

static ssize_t show_port_ib_mtu(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_mtu_attr);
	struct mlx4_dev *mdev = info->dev;

	if (mdev->caps.port_type[info->port] == MLX4_PORT_TYPE_ETH)
		mlx4_warn(mdev, "port level mtu is only used for IB ports\n");

	sprintf(buf, "%d\n",
			ibta_mtu_to_int(mdev->caps.port_ib_mtu[info->port]));
	return strlen(buf);
}

static ssize_t set_port_ib_mtu(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct mlx4_port_info *info = container_of(attr, struct mlx4_port_info,
						   port_mtu_attr);
	struct mlx4_dev *mdev = info->dev;
	struct mlx4_priv *priv = mlx4_priv(mdev);
	int err, port, mtu, ibta_mtu = -1;

	if (mdev->caps.port_type[info->port] == MLX4_PORT_TYPE_ETH) {
		mlx4_warn(mdev, "port level mtu is only used for IB ports\n");
		return -EINVAL;
	}

	mtu = (int) simple_strtol(buf, NULL, 0);
	ibta_mtu = int_to_ibta_mtu(mtu);

	if (ibta_mtu < 0) {
		mlx4_err(mdev, "%s is invalid IBTA mtu\n", buf);
		return -EINVAL;
	}

	mdev->caps.port_ib_mtu[info->port] = ibta_mtu;

	mlx4_stop_sense(mdev);
	mutex_lock(&priv->port_mutex);
	mlx4_unregister_device(mdev);
	for (port = 1; port <= mdev->caps.num_ports; port++) {
		mlx4_CLOSE_PORT(mdev, port);
		err = mlx4_SET_PORT(mdev, port, -1);
		if (err) {
			mlx4_err(mdev, "Failed to set port %d, "
				      "aborting\n", port);
			goto err_set_port;
		}
	}
	err = mlx4_register_device(mdev);
err_set_port:
	mutex_unlock(&priv->port_mutex);
	mlx4_start_sense(mdev);
	return err ? err : count;
}

static int mlx4_load_fw(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err, unmap_flag = 0;

	priv->fw.fw_icm = mlx4_alloc_icm(dev, priv->fw.fw_pages,
					 GFP_HIGHUSER | __GFP_NOWARN, 0);
	if (!priv->fw.fw_icm) {
		mlx4_err(dev, "Couldn't allocate FW area, aborting.\n");
		return -ENOMEM;
	}

	err = mlx4_MAP_FA(dev, priv->fw.fw_icm);
	if (err) {
		mlx4_err(dev, "MAP_FA command failed, aborting.\n");
		goto err_free;
	}

	err = mlx4_RUN_FW(dev);
	if (err) {
		mlx4_err(dev, "RUN_FW command failed, aborting.\n");
		goto err_unmap_fa;
	}

	return 0;

err_unmap_fa:
	unmap_flag = mlx4_UNMAP_FA(dev);
	if (unmap_flag)
		pr_warn("mlx4_core: mlx4_UNMAP_FA failed.\n");

err_free:
	if (!unmap_flag)
		mlx4_free_icm(dev, priv->fw.fw_icm, 0);
	return err;
}

static int mlx4_init_cmpt_table(struct mlx4_dev *dev, u64 cmpt_base,
				int cmpt_entry_sz)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;
	int num_eqs;

	err = mlx4_init_icm_table(dev, &priv->qp_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_QP *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err)
		goto err;

	err = mlx4_init_icm_table(dev, &priv->srq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_SRQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_srqs,
				  dev->caps.reserved_srqs, 0, 0);
	if (err)
		goto err_qp;

	err = mlx4_init_icm_table(dev, &priv->cq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_CQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, dev->caps.num_cqs,
				  dev->caps.reserved_cqs, 0, 0);
	if (err)
		goto err_srq;

	num_eqs = dev->phys_caps.num_phys_eqs;
	err = mlx4_init_icm_table(dev, &priv->eq_table.cmpt_table,
				  cmpt_base +
				  ((u64) (MLX4_CMPT_TYPE_EQ *
					  cmpt_entry_sz) << MLX4_CMPT_SHIFT),
				  cmpt_entry_sz, num_eqs, num_eqs, 0, 0);
	if (err)
		goto err_cq;

	return 0;

err_cq:
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);

err_srq:
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);

err_qp:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

err:
	return err;
}

static int mlx4_init_icm(struct mlx4_dev *dev, struct mlx4_dev_cap *dev_cap,
			 struct mlx4_init_hca_param *init_hca, u64 icm_size)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	u64 aux_pages;
	int num_eqs;
	int err, unmap_flag = 0;

	err = mlx4_SET_ICM_SIZE(dev, icm_size, &aux_pages);
	if (err) {
		mlx4_err(dev, "SET_ICM_SIZE command failed, aborting.\n");
		return err;
	}

	mlx4_dbg(dev, "%lld KB of HCA context requires %lld KB aux memory.\n",
		 (unsigned long long) icm_size >> 10,
		 (unsigned long long) aux_pages << 2);

	priv->fw.aux_icm = mlx4_alloc_icm(dev, aux_pages,
					  GFP_HIGHUSER | __GFP_NOWARN, 0);
	if (!priv->fw.aux_icm) {
		mlx4_err(dev, "Couldn't allocate aux memory, aborting.\n");
		return -ENOMEM;
	}

	err = mlx4_MAP_ICM_AUX(dev, priv->fw.aux_icm);
	if (err) {
		mlx4_err(dev, "MAP_ICM_AUX command failed, aborting.\n");
		goto err_free_aux;
	}

	err = mlx4_init_cmpt_table(dev, init_hca->cmpt_base, dev_cap->cmpt_entry_sz);
	if (err) {
		mlx4_err(dev, "Failed to map cMPT context memory, aborting.\n");
		goto err_unmap_aux;
	}

	num_eqs = dev->phys_caps.num_phys_eqs;
	err = mlx4_init_icm_table(dev, &priv->eq_table.table,
				  init_hca->eqc_base, dev_cap->eqc_entry_sz,
				  num_eqs, num_eqs, 0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map EQ context memory, aborting.\n");
		goto err_unmap_cmpt;
	}

	/*
	 * Reserved MTT entries must be aligned up to a cacheline
	 * boundary, since the FW will write to them, while the driver
	 * writes to all other MTT entries. (The variable
	 * dev->caps.mtt_entry_sz below is really the MTT segment
	 * size, not the raw entry size)
	 */
	dev->caps.reserved_mtts =
		ALIGN(dev->caps.reserved_mtts * dev->caps.mtt_entry_sz,
		      dma_get_cache_alignment()) / dev->caps.mtt_entry_sz;

	err = mlx4_init_icm_table(dev, &priv->mr_table.mtt_table,
				  init_hca->mtt_base,
				  dev->caps.mtt_entry_sz,
				  dev->caps.num_mtts,
				  dev->caps.reserved_mtts, 1, 0);
	if (err) {
		mlx4_err(dev, "Failed to map MTT context memory, aborting.\n");
		goto err_unmap_eq;
	}

	err = mlx4_init_icm_table(dev, &priv->mr_table.dmpt_table,
				  init_hca->dmpt_base,
				  dev_cap->dmpt_entry_sz,
				  dev->caps.num_mpts,
				  dev->caps.reserved_mrws, 1, 1);
	if (err) {
		mlx4_err(dev, "Failed to map dMPT context memory, aborting.\n");
		goto err_unmap_mtt;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.qp_table,
				  init_hca->qpc_base,
				  dev_cap->qpc_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map QP context memory, aborting.\n");
		goto err_unmap_dmpt;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.auxc_table,
				  init_hca->auxc_base,
				  dev_cap->aux_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map AUXC context memory, aborting.\n");
		goto err_unmap_qp;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.altc_table,
				  init_hca->altc_base,
				  dev_cap->altc_entry_sz,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map ALTC context memory, aborting.\n");
		goto err_unmap_auxc;
	}

	err = mlx4_init_icm_table(dev, &priv->qp_table.rdmarc_table,
				  init_hca->rdmarc_base,
				  dev_cap->rdmarc_entry_sz << priv->qp_table.rdmarc_shift,
				  dev->caps.num_qps,
				  dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW],
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map RDMARC context memory, aborting\n");
		goto err_unmap_altc;
	}

	err = mlx4_init_icm_table(dev, &priv->cq_table.table,
				  init_hca->cqc_base,
				  dev_cap->cqc_entry_sz,
				  dev->caps.num_cqs,
				  dev->caps.reserved_cqs, 0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map CQ context memory, aborting.\n");
		goto err_unmap_rdmarc;
	}

	err = mlx4_init_icm_table(dev, &priv->srq_table.table,
				  init_hca->srqc_base,
				  dev_cap->srq_entry_sz,
				  dev->caps.num_srqs,
				  dev->caps.reserved_srqs, 0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map SRQ context memory, aborting.\n");
		goto err_unmap_cq;
	}

	/*
	 * For flow steering device managed mode it is required to use
	 * mlx4_init_icm_table. For B0 steering mode it's not strictly
	 * required, but for simplicity just map the whole multicast
	 * group table now.  The table isn't very big and it's a lot
	 * easier than trying to track ref counts.
	 */
	err = mlx4_init_icm_table(dev, &priv->mcg_table.table,
				  init_hca->mc_base,
				  mlx4_get_mgm_entry_size(dev),
				  dev->caps.num_mgms + dev->caps.num_amgms,
				  dev->caps.num_mgms + dev->caps.num_amgms,
				  0, 0);
	if (err) {
		mlx4_err(dev, "Failed to map MCG context memory, aborting.\n");
		goto err_unmap_srq;
	}

	return 0;

err_unmap_srq:
	mlx4_cleanup_icm_table(dev, &priv->srq_table.table);

err_unmap_cq:
	mlx4_cleanup_icm_table(dev, &priv->cq_table.table);

err_unmap_rdmarc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.rdmarc_table);

err_unmap_altc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.altc_table);

err_unmap_auxc:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.auxc_table);

err_unmap_qp:
	mlx4_cleanup_icm_table(dev, &priv->qp_table.qp_table);

err_unmap_dmpt:
	mlx4_cleanup_icm_table(dev, &priv->mr_table.dmpt_table);

err_unmap_mtt:
	mlx4_cleanup_icm_table(dev, &priv->mr_table.mtt_table);

err_unmap_eq:
	mlx4_cleanup_icm_table(dev, &priv->eq_table.table);

err_unmap_cmpt:
	mlx4_cleanup_icm_table(dev, &priv->eq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

err_unmap_aux:
	unmap_flag = mlx4_UNMAP_ICM_AUX(dev);
	if (unmap_flag)
		pr_warn("mlx4_core: mlx4_UNMAP_ICM_AUX failed.\n");

err_free_aux:
	if (!unmap_flag)
		mlx4_free_icm(dev, priv->fw.aux_icm, 0);

	return err;
}

static void mlx4_free_icms(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	mlx4_cleanup_icm_table(dev, &priv->mcg_table.table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.rdmarc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.altc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.auxc_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.qp_table);
	mlx4_cleanup_icm_table(dev, &priv->mr_table.dmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->mr_table.mtt_table);
	mlx4_cleanup_icm_table(dev, &priv->eq_table.table);
	mlx4_cleanup_icm_table(dev, &priv->eq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->cq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->srq_table.cmpt_table);
	mlx4_cleanup_icm_table(dev, &priv->qp_table.cmpt_table);

	if (!mlx4_UNMAP_ICM_AUX(dev))
		mlx4_free_icm(dev, priv->fw.aux_icm, 0);
	else
		pr_warn("mlx4_core: mlx4_UNMAP_ICM_AUX failed.\n");
}

static void mlx4_slave_exit(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	mutex_lock(&priv->cmd.slave_cmd_mutex);
	if (mlx4_comm_cmd(dev, MLX4_COMM_CMD_RESET, 0,
			  MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME))
		mlx4_warn(dev, "Failed to close slave function.\n");
	mutex_unlock(&priv->cmd.slave_cmd_mutex);
}

static int map_bf_area(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	resource_size_t bf_start;
	resource_size_t bf_len;
	int err = 0;

	if (!dev->caps.bf_reg_size)
		return -ENXIO;

	bf_start = pci_resource_start(dev->pdev, 2) +
			(dev->caps.num_uars << PAGE_SHIFT);
	bf_len = pci_resource_len(dev->pdev, 2) -
			(dev->caps.num_uars << PAGE_SHIFT);
	priv->bf_mapping = io_mapping_create_wc(bf_start, bf_len);
	if (!priv->bf_mapping)
		err = -ENOMEM;

	return err;
}

static void unmap_bf_area(struct mlx4_dev *dev)
{
	if (mlx4_priv(dev)->bf_mapping)
		io_mapping_free(mlx4_priv(dev)->bf_mapping);
}

cycle_t mlx4_read_clock(struct mlx4_dev *dev)
{
	u32 clockhi, clocklo, clockhi1;
	cycle_t cycles;
	int i;
	struct mlx4_priv *priv = mlx4_priv(dev);

	if (!priv->clock_mapping)
		return -ENOTSUPP;

	for (i = 0; i < 10; i++) {
		clockhi = be32_to_cpu(readl(priv->clock_mapping)) & 0xffff;
		clocklo = be32_to_cpu(readl(priv->clock_mapping + 4));
		clockhi1 = be32_to_cpu(readl(priv->clock_mapping)) & 0xffff;
		if (clockhi == clockhi1)
			break;
	}

	cycles = (u64)clockhi << 32 | (u64)clocklo;

	return cycles;
}
EXPORT_SYMBOL_GPL(mlx4_read_clock);


int map_internal_clock(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	priv->clock_mapping = ioremap(pci_resource_start(dev->pdev,
				priv->fw.clock_bar) +
				priv->fw.clock_offset, MLX4_CLOCK_SIZE);

	if (!priv->clock_mapping)
		return -ENOMEM;

	return 0;
}


int mlx4_get_internal_clock_params(struct mlx4_dev *dev,
				   struct mlx4_clock_params *params)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	if (mlx4_is_slave(dev))
		return -ENOTSUPP;
	if (!params)
		return -EINVAL;

	params->bar = priv->fw.clock_bar;
	params->offset = priv->fw.clock_offset;
	params->size = MLX4_CLOCK_SIZE;

	return 0;
}
EXPORT_SYMBOL_GPL(mlx4_get_internal_clock_params);

void unmap_internal_clock(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	if (priv->clock_mapping)
		iounmap(priv->clock_mapping);
}

static void mlx4_close_hca(struct mlx4_dev *dev)
{
	unmap_internal_clock(dev);
	unmap_bf_area(dev);
	if (mlx4_is_slave(dev)) {
		mlx4_slave_exit(dev);
	} else {
		mlx4_CLOSE_HCA(dev, 0);
		mlx4_free_icms(dev);

		if (!mlx4_UNMAP_FA(dev))
			 mlx4_free_icm(dev, mlx4_priv(dev)->fw.fw_icm, 0);
		else
			pr_warn("mlx4_core: mlx4_UNMAP_FA failed.\n");
	}
}

static int mlx4_comm_check_offline(struct mlx4_dev *dev)
{
#define COMM_CHAN_OFFLINE_OFFSET 0x09

	u32 comm_flags;
	u32 offline_bit;
	unsigned long end;
	struct mlx4_priv *priv = mlx4_priv(dev);

	end = msecs_to_jiffies(MLX4_COMM_OFFLINE_TIME_OUT) + jiffies;
	while (time_before(jiffies, end)) {
		comm_flags = swab32(readl((void *)priv->mfunc.comm +
					  MLX4_COMM_CHAN_FLAGS));
		offline_bit = (comm_flags &
			       (u32)(1 << COMM_CHAN_OFFLINE_OFFSET));
		if (!offline_bit)
			return 0;
		else
			/* There are cases as part of AER/Reset flow that PF needs around,
			  * 100 milli seconds to load, letting other tasks on that CPU to run by that time.
			*/
			msleep(100);
	}
	mlx4_err(dev, "Communication channel is offline.\n");
	return -EIO;
}

static void mlx4_reset_vf_support(struct mlx4_dev *dev)
{
#define COMM_CHAN_RST_OFFSET 0x1e

	struct mlx4_priv *priv = mlx4_priv(dev);
	u32 comm_rst;
	u32 comm_caps;

	comm_caps = swab32(readl((void *)priv->mfunc.comm +
				 MLX4_COMM_CHAN_CAPS));
	comm_rst = (comm_caps & (u32)(1 << COMM_CHAN_RST_OFFSET));

	if (comm_rst)
		dev->caps.vf_reset = 1;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
static atomic_t pf_loading = ATOMIC_INIT(0);
#endif

static int mlx4_init_slave(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	u64 dma = (u64) priv->mfunc.vhcr_dma;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
	int num_of_reset_retries = NUM_OF_RESET_RETRIES;
#endif
	int ret_from_reset = 0;
	u32 slave_read;
	u32 cmd_channel_ver;

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
	if (atomic_read(&pf_loading)) {
		mlx4_warn(dev, "PF is not ready. Deferring probe\n");
		return -EPROBE_DEFER;
	}
#endif

	mutex_lock(&priv->cmd.slave_cmd_mutex);
	priv->cmd.max_cmds = 1;

	if (mlx4_comm_check_offline(dev)) {
		mlx4_err(dev, "PF is not responsive, skipping initialization\n");
		goto err_offline;
	}
	mlx4_reset_vf_support(dev);

	mlx4_warn(dev, "Sending reset\n");
	ret_from_reset = mlx4_comm_cmd(dev, MLX4_COMM_CMD_RESET, 0,
				       MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME);
	/* if we are in the middle of flr the slave will try
	 * NUM_OF_RESET_RETRIES times before leaving.*/
	if (ret_from_reset) {
		if (MLX4_DELAY_RESET_SLAVE == ret_from_reset) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
			mlx4_warn(dev, "slave is currently in the "
				  "middle of FLR. Deferring probe.\n");
			mutex_unlock(&priv->cmd.slave_cmd_mutex);
			return -EPROBE_DEFER;
#else
                        msleep(SLEEP_TIME_IN_RESET);
                       	while (ret_from_reset && num_of_reset_retries) {
                               	mlx4_warn(dev, "slave is currently in the"
                                         "middle of FLR. retrying..."
                                         "(try num:%d)\n",
                                         (NUM_OF_RESET_RETRIES -
                                          num_of_reset_retries  + 1));
                               	ret_from_reset =
                                       mlx4_comm_cmd(dev, MLX4_COMM_CMD_RESET,
                                                      0, MLX4_COMM_CMD_NA_OP,
                                                      MLX4_COMM_TIME);
				num_of_reset_retries = num_of_reset_retries - 1;
                       }
#endif
		} else
			goto err;
	}

	/* check the driver version - the slave I/F revision
	 * must match the master's */
	slave_read = swab32(readl(&priv->mfunc.comm->slave_read));
	cmd_channel_ver = mlx4_comm_get_version();

	if (MLX4_COMM_GET_IF_REV(cmd_channel_ver) !=
		MLX4_COMM_GET_IF_REV(slave_read)) {
		mlx4_err(dev, "slave driver version is not supported"
			 " by the master\n");
		goto err;
	}

	mlx4_warn(dev, "Sending vhcr0\n");
	if (mlx4_comm_cmd(dev, MLX4_COMM_CMD_VHCR0, dma >> 48,
			  MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME))
		goto err;
	if (mlx4_comm_cmd(dev, MLX4_COMM_CMD_VHCR1, dma >> 32,
			  MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME))
		goto err;
	if (mlx4_comm_cmd(dev, MLX4_COMM_CMD_VHCR2, dma >> 16,
			  MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME))
		goto err;
	if (mlx4_comm_cmd(dev, MLX4_COMM_CMD_VHCR_EN, dma,
			  MLX4_COMM_CMD_NA_OP, MLX4_COMM_TIME))
		goto err;

	mutex_unlock(&priv->cmd.slave_cmd_mutex);
	return 0;

err:
	mlx4_comm_cmd(dev, MLX4_COMM_CMD_RESET, 0,
		      MLX4_COMM_CMD_NA_OP, 0);
err_offline:
	mutex_unlock(&priv->cmd.slave_cmd_mutex);
	return -EIO;
}

static void mlx4_parav_master_pf_caps(struct mlx4_dev *dev)
{
	int i;

	for (i = 1; i <= dev->caps.num_ports; i++) {
		dev->caps.gid_table_len[i] =
			mlx4_get_slave_num_gids(dev, 0, i);
		dev->caps.pkey_table_len[i] =
			dev->phys_caps.pkey_phys_table_len[i] - 1;
	}
}

static int choose_log_fs_mgm_entry_size(int qp_per_entry)
{
	int i = MLX4_MIN_MGM_LOG_ENTRY_SIZE;

	for (i = MLX4_MIN_MGM_LOG_ENTRY_SIZE; i <= MLX4_MAX_MGM_LOG_ENTRY_SIZE;
	      i++) {
		if (qp_per_entry <= 4 * ((1 << i) / 16 - 2))
			break;
	}

	return (i <= MLX4_MAX_MGM_LOG_ENTRY_SIZE) ? i : -1;
}

static const char *dmfs_high_rate_steering_mode_str(int dmfs_high_steer_mode)
{
	switch (dmfs_high_steer_mode) {
	case MLX4_STEERING_DMFS_A0_DEFAULT:
		return "performance optimized";

	case MLX4_STEERING_DMFS_A0_DYNAMIC:
		return "dynamic hybrid mode";

	case MLX4_STEERING_DMFS_A0_STATIC:
		return "performance optimized for limited rule configuration";

	case MLX4_STEERING_DMFS_A0_DISABLE:
		return "disabled performance optimized steering";

	case MLX4_STEERING_DMFS_A0_NOT_SUPPORTED:
		return "performance optimized steering not supported";

	default:
		return "Unrecognized mode";
	}
}

#define MLX4_DMFS_LOW_QP_COUNT			63

static void choose_steering_mode(struct mlx4_dev *dev,
				 struct mlx4_dev_cap *dev_cap)
{
	int nvfs;
	int mlx4_current_steering_mode = mlx4_log_num_mgm_entry_size;

	mlx4_get_val(num_vfs.dbdf2val.tbl, pci_physfn(dev->pdev), 0, &nvfs);
	if (high_rate_steer && !mlx4_is_mfunc(dev)) {
		dev->caps.flags &= ~(MLX4_DEV_CAP_FLAG_VEP_MC_STEER |
				     MLX4_DEV_CAP_FLAG_VEP_UC_STEER);
		dev_cap->flags2 &= ~MLX4_DEV_CAP_FLAG2_FS_EN;
	}

	/* DMFS by default requires NCSI support with DMFS */
	if (mlx4_current_steering_mode <= 0) {
		if (!((-mlx4_current_steering_mode) & MLX4_FORCE_DMFS_IF_NO_NCSI_FS))
			if (!(dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_FS_EN_NCSI))
				mlx4_current_steering_mode =
					MLX4_DEFAULT_MGM_LOG_ENTRY_SIZE;

		if ((-mlx4_current_steering_mode) & MLX4_DISABLE_DMFS_LOW_QP_NUM)
			if (dev_cap->fs_max_num_qp_per_entry <= MLX4_DMFS_LOW_QP_COUNT) {
				mlx4_warn(dev, "FW supports only %d QPs per mcg entry, "
					  "falling back to B0\n",
					  dev_cap->fs_max_num_qp_per_entry);
				mlx4_current_steering_mode =
					MLX4_DEFAULT_MGM_LOG_ENTRY_SIZE;
			}

		if ((-mlx4_current_steering_mode) & MLX4_DMFS_A0_STEERING) {
			if (dev->caps.dmfs_high_steer_mode ==
			    MLX4_STEERING_DMFS_A0_NOT_SUPPORTED)
				mlx4_err(dev, "DMFS high rate mode not supported\n");
			else
				dev->caps.dmfs_high_steer_mode =
					MLX4_STEERING_DMFS_A0_STATIC;
		}
	}

	dev->caps.steering_attr = 0;

	if (mlx4_current_steering_mode <= 0 &&
	    dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_FS_EN &&
	    (!mlx4_is_mfunc(dev) ||
	     (dev_cap->fs_max_num_qp_per_entry >= (dev->num_vfs + 1))) &&
	    choose_log_fs_mgm_entry_size(dev_cap->fs_max_num_qp_per_entry) >=
		MLX4_MIN_MGM_LOG_ENTRY_SIZE) {
		dev->oper_log_mgm_entry_size =
			choose_log_fs_mgm_entry_size(dev_cap->fs_max_num_qp_per_entry);
		dev->caps.steering_mode = MLX4_STEERING_MODE_DEVICE_MANAGED;
		dev->caps.num_qp_per_mgm = dev_cap->fs_max_num_qp_per_entry;

		dev->caps.steering_attr |= MLX4_STEERING_ATTR_DMFS_EN;

		if (dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_DMFS_IPOIB &&
		    (!((-mlx4_current_steering_mode) & MLX4_DMFS_ETH_ONLY)))
			dev->caps.steering_attr |= MLX4_STEERING_ATTR_DMFS_IPOIB;

	} else {
		if (dev->caps.dmfs_high_steer_mode !=
		    MLX4_STEERING_DMFS_A0_NOT_SUPPORTED)
			dev->caps.dmfs_high_steer_mode = MLX4_STEERING_DMFS_A0_DISABLE;
		if (dev->caps.flags & MLX4_DEV_CAP_FLAG_VEP_UC_STEER &&
		    dev->caps.flags & MLX4_DEV_CAP_FLAG_VEP_MC_STEER)
			dev->caps.steering_mode = MLX4_STEERING_MODE_B0;
		else {
			dev->caps.steering_mode = MLX4_STEERING_MODE_A0;

			if (dev->caps.flags & MLX4_DEV_CAP_FLAG_VEP_UC_STEER ||
			    dev->caps.flags & MLX4_DEV_CAP_FLAG_VEP_MC_STEER)
				mlx4_warn(dev, "Must have both UC_STEER and MC_STEER flags "
					  "set to use B0 steering. Falling back to A0 steering mode.\n");
		}
		dev->oper_log_mgm_entry_size =
			mlx4_current_steering_mode > 0 ?
			mlx4_current_steering_mode :
			MLX4_DEFAULT_MGM_LOG_ENTRY_SIZE;
		dev->caps.num_qp_per_mgm = mlx4_get_qp_per_mgm(dev);
	}
	mlx4_dbg(dev, "Steering mode is: %s, oper_log_mgm_entry_size = %d, "
		 "current_steering_mode = %d\n",
		 mlx4_steering_mode_str(dev->caps.steering_mode),
		 dev->oper_log_mgm_entry_size, mlx4_current_steering_mode);
}

static void choose_tunnel_offload_mode(struct mlx4_dev *dev,
				       struct mlx4_dev_cap *dev_cap)
{
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (dev->caps.steering_mode == MLX4_STEERING_MODE_DEVICE_MANAGED &&
	    dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_VXLAN_OFFLOADS &&
	    dev->caps.dmfs_high_steer_mode != MLX4_STEERING_DMFS_A0_STATIC)
		dev->caps.tunnel_offload_mode = MLX4_TUNNEL_OFFLOAD_MODE_VXLAN;
	else
#endif
		dev->caps.tunnel_offload_mode = MLX4_TUNNEL_OFFLOAD_MODE_NONE;

	mlx4_dbg(dev, "Tunneling offload mode is: %s\n",
		 (dev->caps.tunnel_offload_mode ==
		 MLX4_TUNNEL_OFFLOAD_MODE_VXLAN) ? "vxlan" : "none");
}

static int mlx4_init_fw(struct mlx4_dev *dev)
{
	struct mlx4_mod_stat_cfg mlx4_cfg;
	int err;

	if (!mlx4_is_slave(dev)) {
		err = mlx4_QUERY_FW(dev);
		if (err) {
			if (err == -EACCES)
				mlx4_info(dev, "non-primary physical function, skipping.\n");
			else
				mlx4_err(dev, "QUERY_FW command failed, err=%d, aborting.\n", err);
			return err;
		}

		err = mlx4_load_fw(dev);
		if (err) {
			mlx4_err(dev, "Failed to start FW, aborting.\n");
			return err;
		}

		mlx4_cfg.log_pg_sz_m = 1;
		mlx4_cfg.log_pg_sz = 0;
		err = mlx4_MOD_STAT_CFG(dev, &mlx4_cfg);
		if (err)
			mlx4_warn(dev, "Failed to override log_pg_sz parameter\n");
		return err;
	}

	return 0;
}

static int mlx4_validate_optimized_steering(struct mlx4_dev *dev)
{
	int i;
	struct mlx4_port_cap port_cap;
	if (dev->caps.dmfs_high_steer_mode == MLX4_STEERING_DMFS_A0_NOT_SUPPORTED)
		return -EINVAL;


	for (i = 1; i <= dev->caps.num_ports; i++) {
		if (mlx4_dev_port(dev, i, &port_cap)) {
			mlx4_err(dev,
				 "QUERY_DEV_CAP command failed, can't veify DMFS high rate steering.\n");
		} else if ((dev->caps.dmfs_high_steer_mode !=
			    MLX4_STEERING_DMFS_A0_DEFAULT) &&
			   (port_cap.dmfs_optimized_state ==
			   !!(dev->caps.dmfs_high_steer_mode ==
			    MLX4_STEERING_DMFS_A0_DISABLE))) {
			mlx4_err(dev,
				 "DMFS high rate steer mode differ, driver requested %s but %s in FW.\n",
				 dmfs_high_rate_steering_mode_str(
					dev->caps.dmfs_high_steer_mode),
				 (port_cap.dmfs_optimized_state ?
					"enabled" : "disabled"));
		}
	}

	return 0;
}

static int mlx4_init_hca(struct mlx4_dev *dev)
{
	struct mlx4_priv	  *priv = mlx4_priv(dev);
	struct mlx4_dev_cap	   *dev_cap = NULL;
	struct mlx4_adapter	   adapter;
	struct mlx4_profile	   profile;
	struct mlx4_init_hca_param init_hca;
	u64 icm_size;
	int err;
	struct mlx4_rx_csum_params params;

	if (!mlx4_is_slave(dev)) {
		dev_cap = kzalloc(sizeof *dev_cap, GFP_KERNEL);
		if (!dev_cap) {
			mlx4_err(dev, "Failed to allocate memory for dev_cap\n");
			err = -ENOMEM;
			goto err_stop_fw;
		}

		err = mlx4_dev_cap(dev, dev_cap);
		if (err) {
			mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
			goto err_stop_fw;
		}

		choose_steering_mode(dev, dev_cap);
		choose_tunnel_offload_mode(dev, dev_cap);
		choose_roce_mode(dev, dev_cap);
		if (dev->caps.roce_mode == MLX4_ROCE_MODE_1_5) {
			if (!mlx4_roce_proto_config) {
				dev->caps.roce_proto_config = MLX4_ROCE_1_5_DEF_PROTO;
			} else {
				if (mlx4_roce_proto_config > 0xff) {
					mlx4_warn(dev, "Illegal value for rr_proto (%d), using %d\n",
						  mlx4_roce_proto_config,
						  MLX4_ROCE_MODE_1_5);
					dev->caps.roce_proto_config =
						MLX4_ROCE_1_5_DEF_PROTO;
				} else {
					dev->caps.roce_proto_config =
						mlx4_roce_proto_config;
				}
			}
		}

		if (dev->caps.roce_mode == MLX4_ROCE_MODE_2) {
			if (!mlx4_roce_proto_config) {
				dev->caps.roce_proto_config = MLX4_ROCE_2_DEF_PROTO;
			} else {
				if (mlx4_roce_proto_config > 0xffff) {
					mlx4_warn(dev, "Illegal value for rr_proto (%d), using %d\n",
						  mlx4_roce_proto_config,
						  MLX4_ROCE_2_DEF_PROTO);
					dev->caps.roce_proto_config =
						MLX4_ROCE_2_DEF_PROTO;
				} else {
					dev->caps.roce_proto_config =
						mlx4_roce_proto_config;
				}
			}
		}

		if (mlx4_is_master(dev))
			mlx4_parav_master_pf_caps(dev);

		process_mod_param_profile(&profile);
		if (dev->caps.steering_mode ==
		    MLX4_STEERING_MODE_DEVICE_MANAGED)
			profile.num_mcg = MLX4_FS_NUM_MCG;

		icm_size = mlx4_make_profile(dev, &profile, dev_cap,
					     &init_hca);
		if ((long long) icm_size < 0) {
			err = icm_size;
			goto err_stop_fw;
		}

		dev->caps.max_fmr_maps = (1 << (32 - ilog2(dev->caps.num_mpts))) - 1;

		init_hca.log_uar_sz = ilog2(dev->caps.num_uars);
		init_hca.uar_page_sz = PAGE_SHIFT - 12;

		err = mlx4_init_icm(dev, dev_cap, &init_hca, icm_size);
		if (err)
			goto err_stop_fw;

		init_hca.mw_enable = 1;

		err = mlx4_INIT_HCA(dev, &init_hca);
		if (err) {
			mlx4_err(dev, "INIT_HCA command failed, aborting.\n");
			goto err_free_icm;
		}

		if (dev_cap->num_sys_eqs) {
			err = mlx4_query_func(dev, dev_cap);
			if (err) {
				mlx4_err(dev, "QUERY_FUNC command failed, aborting.\n");
				goto err_stop_fw;
			}
		}

		/*
		 * Read HCA frequency by QUERY_HCA command
		 */
		if (dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS) {
			memset(&init_hca, 0, sizeof(init_hca));
			err = mlx4_QUERY_HCA(dev, &init_hca);
			if (err) {
				mlx4_err(dev, "QUERY_HCA command failed, disable timestamp.\n");
				dev->caps.flags2 &= ~MLX4_DEV_CAP_FLAG2_TS;
			} else {
				dev->caps.hca_core_clock =
					init_hca.hca_core_clock;
			}

			/* In case we got HCA frequency 0 - disable timestamping
			 * to avoid dividing by zero
			 */
			if (!dev->caps.hca_core_clock) {
				dev->caps.flags2 &= ~MLX4_DEV_CAP_FLAG2_TS;
				mlx4_err(dev, "HCA frequency is 0. Timestamping is not supported.");
			} else if (map_internal_clock(dev)) {
				/* Map internal clock,
				 * in case of failure disable timestamping
				 */
				dev->caps.flags2 &= ~MLX4_DEV_CAP_FLAG2_TS;
				mlx4_err(dev, "Failed to map internal clock. Timestamping is not supported.\n");
			}
		}

		if (dev->caps.dmfs_high_steer_mode !=
		    MLX4_STEERING_DMFS_A0_NOT_SUPPORTED) {
			if (mlx4_validate_optimized_steering(dev))
				mlx4_warn(dev, "Optimized steering validation failed\n");

			if (dev->caps.dmfs_high_steer_mode ==
			    MLX4_STEERING_DMFS_A0_DISABLE) {
				dev->caps.dmfs_high_rate_qpn_base =
					dev->caps.reserved_qps_cnt[MLX4_QP_REGION_FW];
				dev->caps.dmfs_high_rate_qpn_range =
					MLX4_A0_STEERING_TABLE_SIZE;
			}

			mlx4_dbg(dev, "DMFS high rate steer mode is \"%s\"\n",
				 dmfs_high_rate_steering_mode_str(
					dev->caps.dmfs_high_steer_mode));
		}
	} else {
		err = mlx4_init_slave(dev);
		if (err) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
			if (err != -EPROBE_DEFER)
				mlx4_err(dev, "Failed to initialize slave\n");
#else
			mlx4_err(dev, "Failed to initialize slave\n");
#endif
			return err;
		}

		err = mlx4_slave_cap(dev);
		if (err) {
			mlx4_err(dev, "Failed to obtain slave caps\n");
			goto err_close;
		}
	}
	mlx4_dbg(dev, "RoCE mode is %s\n", mlx4_roce_mode_str(dev->caps.roce_mode));

	if (map_bf_area(dev))
		mlx4_dbg(dev, "Failed to map blue flame area\n");

	/* Only the master set the ports, all the rest got it from it.*/
	if (!mlx4_is_slave(dev))
		mlx4_set_port_mask(dev);

	err = mlx4_QUERY_ADAPTER(dev, &adapter);
	if (err) {
		mlx4_err(dev, "QUERY_ADAPTER command failed, aborting.\n");
		goto unmap_bf;
	}

	/* Query CONFIG_DEV parameters */
	err = mlx4_config_dev_rx_flags_retrieval(dev, &params);
	if (err) {
		if (err == -ENOTSUPP)
			mlx4_err(dev, "Querying CONFIG_DEV is not supported\n");
		else
			mlx4_err(dev, "Failed to query CONFIG_DEV parameters\n");
	} else {
		dev->caps.rx_checksum_flags_port[1] = params.rx_csum_flags_port_1;
		dev->caps.rx_checksum_flags_port[2] = params.rx_csum_flags_port_2;
	}
	priv->eq_table.inta_pin = adapter.inta_pin;
	memcpy(dev->board_id, adapter.board_id, sizeof dev->board_id);
	memcpy(dev->vsd, adapter.vsd, sizeof(dev->vsd));
	dev->vsd_vendor_id = adapter.vsd_vendor_id;

	if (!mlx4_is_slave(dev))
		kfree(dev_cap);

	return 0;

unmap_bf:
	if (!mlx4_is_slave(dev))
		unmap_internal_clock(dev);
	unmap_bf_area(dev);

	if (mlx4_is_slave(dev)) {
		kfree(dev->caps.qp0_qkey);
		kfree(dev->caps.qp0_tunnel);
		kfree(dev->caps.qp0_proxy);
		kfree(dev->caps.qp1_tunnel);
		kfree(dev->caps.qp1_proxy);
	}

err_close:
	if (mlx4_is_slave(dev))
		mlx4_slave_exit(dev);
	else
		mlx4_CLOSE_HCA(dev, 0);

err_free_icm:
	if (!mlx4_is_slave(dev))
		mlx4_free_icms(dev);

err_stop_fw:
	if (!mlx4_is_slave(dev)) {
		if (!mlx4_UNMAP_FA(dev))
			mlx4_free_icm(dev, priv->fw.fw_icm, 0);
		else
			pr_warn("mlx4_core: mlx4_UNMAP_FA failed.\n");
		kfree(dev_cap);
	}
	return err;
}

static int mlx4_init_counters_table(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int nent_pow2, port_indx, vf_index, num_counters;
	int res, index = 0;
	struct counter_index *new_counter_index;


	mutex_init(&priv->counters_table.mutex);

	if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_COUNTERS))
		return -ENOENT;

	if (!mlx4_is_slave(dev) &&
	    dev->caps.max_counters == dev->caps.max_extended_counters) {
		res = mlx4_cmd(dev, MLX4_IF_STATE_EXTENDED, 0, 0,
			       MLX4_CMD_SET_IF_STAT,
			       MLX4_CMD_TIME_CLASS_A, MLX4_CMD_NATIVE);
		if (res) {
			mlx4_err(dev, "Failed to set extended counters (err=%d)\n", res);
			return res;
		}
	}

	if (mlx4_is_slave(dev)) {
		for (port_indx = 0; port_indx < dev->caps.num_ports; port_indx++) {
			INIT_LIST_HEAD(&priv->counters_table.global_port_list[port_indx]);
			if (dev->caps.def_counter_index[port_indx] != 0xFF) {
				new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
				if (!new_counter_index)
					return -ENOMEM;
				new_counter_index->index = dev->caps.def_counter_index[port_indx];
				list_add_tail(&new_counter_index->list, &priv->counters_table.global_port_list[port_indx]);
			}
		}
		mlx4_dbg(dev, "%s: slave allocated %d counters for %d ports\n",
			 __func__, dev->caps.num_ports, dev->caps.num_ports);
		return 0;
	}

	nent_pow2 = roundup_pow_of_two(dev->caps.max_counters);

	for (port_indx = 0; port_indx < dev->caps.num_ports; port_indx++) {
		INIT_LIST_HEAD(&priv->counters_table.global_port_list[port_indx]);
		/* allocating 2 counters per port for PFs */
                /* For the PF, the ETH default counters are 0,2; */
		/* and the RoCE default counters are 1,3 */
		for (num_counters = 0; num_counters < 2; num_counters++, index++) {
			new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
			if (!new_counter_index)
				return -ENOMEM;
			new_counter_index->index = index;
			list_add_tail(&new_counter_index->list,
				      &priv->counters_table.global_port_list[port_indx]);
		}
	}

	if (mlx4_is_master(dev)) {
		for (vf_index = 0; vf_index < dev->num_vfs; vf_index++) {
			int slave = mlx4_get_slave_indx(&priv->dev, vf_index);
			struct mlx4_active_ports actv_ports;
			if (slave < 0)
				continue;
			actv_ports = mlx4_get_active_ports(&priv->dev, slave);
			for (port_indx = 0; port_indx < dev->caps.num_ports; port_indx++) {
				INIT_LIST_HEAD(&priv->counters_table.vf_list[vf_index][port_indx]);
				new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
				if (!new_counter_index)
					return -ENOMEM;
				if (index <  nent_pow2 - 2 &&
				    test_bit(port_indx, actv_ports.ports)) {
					new_counter_index->index = index;
					index++;
				} else {
					new_counter_index->index = MLX4_SINK_COUNTER_INDEX;
				}

				list_add_tail(&new_counter_index->list,
					      &priv->counters_table.vf_list[vf_index][port_indx]);
			}
		}

		res = mlx4_bitmap_init(&priv->counters_table.bitmap,
				       nent_pow2, nent_pow2 - 1,
				       index, 1);
		mlx4_dbg(dev, "%s: master allocated %d counters for %d VFs\n",
			 __func__, index, dev->num_vfs);
	} else {
		res = mlx4_bitmap_init(&priv->counters_table.bitmap,
				nent_pow2, nent_pow2 - 1,
				index, 1);
		mlx4_dbg(dev, "%s: native allocated %d counters for %d ports\n",
			 __func__, index, dev->caps.num_ports);
	}

	return 0;

}

static void mlx4_cleanup_counters_table(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int i, j;
	struct counter_index *port, *tmp_port;
	struct counter_index *vf, *tmp_vf;

	mutex_lock(&priv->counters_table.mutex);

	if (dev->caps.flags & MLX4_DEV_CAP_FLAG_COUNTERS) {
		for (i = 0; i < dev->caps.num_ports; i++) {
			list_for_each_entry_safe(port, tmp_port,
						 &priv->counters_table.global_port_list[i],
						 list) {
				list_del(&port->list);
				kfree(port);
			}
		}
		if (mlx4_is_master(dev)) {
			for (i = 0; i < dev->num_vfs; i++) {
				for (j = 0; j < dev->caps.num_ports; j++) {
					list_for_each_entry_safe(vf, tmp_vf,
								 &priv->counters_table.vf_list[i][j],
								 list) {
						/* clear the counter statistic */
						if (__mlx4_clear_if_stat(dev, vf->index))
							mlx4_dbg(dev, "%s: reset counter %d failed\n",
								 __func__, vf->index);
						list_del(&vf->list);
						kfree(vf);
					}
				}
			}
		}
		mlx4_bitmap_cleanup(&priv->counters_table.bitmap);
	}
	mutex_unlock(&priv->counters_table.mutex);
}

int __mlx4_slave_counters_free(struct mlx4_dev *dev, int slave)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int i, first;
	struct counter_index *vf, *tmp_vf;

	/* clean VF's counters for the next useg */
	if (slave > 0 && slave <= dev->num_vfs) {
		mlx4_dbg(dev, "%s: free counters of slave(%d)\n"
			 , __func__, slave);

		mutex_lock(&priv->counters_table.mutex);
		for (i = 0; i < dev->caps.num_ports; i++) {
			first = 0;
			list_for_each_entry_safe(vf, tmp_vf,
						 &priv->counters_table.vf_list[slave - 1][i],
						 list) {
				/* clear the counter statistic */
				if (__mlx4_clear_if_stat(dev, vf->index))
					mlx4_dbg(dev, "%s: reset counter %d failed\n",
						 __func__, vf->index);
				if (first++ && vf->index != MLX4_SINK_COUNTER_INDEX) {
					mlx4_dbg(dev, "%s: delete counter index %d for slave %d and port %d\n"
						 , __func__, vf->index, slave, i + 1);
					mlx4_bitmap_free(&priv->counters_table.bitmap, vf->index, MLX4_USE_RR);
					list_del(&vf->list);
					kfree(vf);
				} else {
					mlx4_dbg(dev, "%s: can't delete default counter index %d for slave %d and port %d\n"
						 , __func__, vf->index, slave, i + 1);
				}
			}
		}
		mutex_unlock(&priv->counters_table.mutex);
	}

	return 0;
}

int __mlx4_counter_alloc(struct mlx4_dev *dev, int slave, int port, u32 *idx)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct counter_index *new_counter_index;

	if (!(dev->caps.flags & MLX4_DEV_CAP_FLAG_COUNTERS))
		return -ENOENT;

	if ((slave > MLX4_MAX_NUM_VF) || (slave < 0) ||
	    (port < 0) || (port > dev->caps.num_ports)) {
		mlx4_dbg(dev, "%s: invalid slave(%d) or port(%d) index\n",
			 __func__, slave, port);
		return -EINVAL;
	}

	/* handle old guest request does not support request by port index */
	if (port == 0) {
		*idx = MLX4_SINK_COUNTER_INDEX;
		mlx4_dbg(dev, "%s: allocated default counter index %d for slave %d port %d\n"
			 , __func__, *idx, slave, port);
		return 0;
	}

	mutex_lock(&priv->counters_table.mutex);

	*idx = mlx4_bitmap_alloc(&priv->counters_table.bitmap);
	/* if no resources return the default counter of the slave and port */
	if (*idx == -1) {
		if (slave == 0) { /* its the ethernet counter ?????? */
			new_counter_index = list_entry(priv->counters_table.global_port_list[port - 1].next,
						       struct counter_index,
						       list);
		} else {
			new_counter_index = list_entry(priv->counters_table.vf_list[slave - 1][port - 1].next,
						       struct counter_index,
						       list);
		}

		*idx = new_counter_index->index;
		mlx4_dbg(dev, "%s: allocated defualt counter index %d for slave %d port %d\n"
			 , __func__, *idx, slave, port);
		goto out;
	}

	if (slave == 0) { /* native or master */
		new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
		if (!new_counter_index)
			goto no_mem;
		new_counter_index->index = *idx;
		list_add_tail(&new_counter_index->list, &priv->counters_table.global_port_list[port - 1]);
	} else {
		new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
		if (!new_counter_index)
			goto no_mem;
		new_counter_index->index = *idx;
		list_add_tail(&new_counter_index->list, &priv->counters_table.vf_list[slave - 1][port - 1]);
	}

	mlx4_dbg(dev, "%s: allocated counter index %d for slave %d port %d\n"
		 , __func__, *idx, slave, port);
out:
	mutex_unlock(&priv->counters_table.mutex);
	return 0;

no_mem:
	mlx4_bitmap_free(&priv->counters_table.bitmap, *idx, MLX4_USE_RR);
	mutex_unlock(&priv->counters_table.mutex);
	*idx = MLX4_SINK_COUNTER_INDEX;
	mlx4_dbg(dev, "%s: failed err (%d)\n"
		 , __func__, -ENOMEM);
	return -ENOMEM;
}

int mlx4_counter_alloc(struct mlx4_dev *dev, u8 port, u32 *idx)
{
	u64 out_param;
	int err;
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct counter_index *new_counter_index, *c_index;

	if (mlx4_is_mfunc(dev)) {
		err = mlx4_cmd_imm(dev, 0, &out_param,
				   ((u32) port) << 8 | (u32) RES_COUNTER,
				   RES_OP_RESERVE, MLX4_CMD_ALLOC_RES,
				   MLX4_CMD_TIME_CLASS_A, MLX4_CMD_WRAPPED);
		if (!err) {
			*idx = get_param_l(&out_param);
			if (*idx == MLX4_SINK_COUNTER_INDEX)
				return -ENOSPC;

			mutex_lock(&priv->counters_table.mutex);
			c_index = list_entry(priv->counters_table.global_port_list[port - 1].next,
					     struct counter_index,
					     list);
			mutex_unlock(&priv->counters_table.mutex);
			if (c_index->index == *idx)
				return -EEXIST;

			if (mlx4_is_slave(dev)) {
				new_counter_index = kmalloc(sizeof(struct counter_index), GFP_KERNEL);
				if (!new_counter_index) {
					mlx4_counter_free(dev, port, *idx);
					return -ENOMEM;
				}
				new_counter_index->index = *idx;
				mutex_lock(&priv->counters_table.mutex);
				list_add_tail(&new_counter_index->list, &priv->counters_table.global_port_list[port - 1]);
				mutex_unlock(&priv->counters_table.mutex);
				mlx4_dbg(dev, "%s: allocated counter index %d for port %d\n"
					 , __func__, *idx, port);
			}
		}
		return err;
	}
	return __mlx4_counter_alloc(dev, 0, port, idx);
}
EXPORT_SYMBOL_GPL(mlx4_counter_alloc);

void __mlx4_counter_free(struct mlx4_dev *dev, int slave, int port, u32 idx)
{
	/* check if native or slave and deletes acordingly */
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct counter_index *pf, *tmp_pf;
	struct counter_index *vf, *tmp_vf;
	int first;


	if (idx == MLX4_SINK_COUNTER_INDEX) {
		mlx4_dbg(dev, "%s: try to delete default counter index %d for port %d\n"
			 , __func__, idx, port);
			return;
	}

	if ((slave > MLX4_MAX_NUM_VF) || (slave < 0) ||
	    (port < 0) || (port > MLX4_MAX_PORTS)) {
		mlx4_warn(dev, "%s: deletion failed due to invalid slave(%d) or port(%d) index\n"
			 , __func__, slave, idx);
			return;
	}

	mutex_lock(&priv->counters_table.mutex);
	if (slave == 0) {
		first = 0;
		list_for_each_entry_safe(pf, tmp_pf,
					 &priv->counters_table.global_port_list[port - 1],
					 list) {
			/* the first 2 counters are reserved */
			if (pf->index == idx) {
				/* clear the counter statistic */
				if (__mlx4_clear_if_stat(dev, pf->index))
					mlx4_dbg(dev, "%s: reset counter %d failed\n",
						 __func__, pf->index);
				if (1 < first && idx != MLX4_SINK_COUNTER_INDEX) {
					list_del(&pf->list);
					kfree(pf);
					mlx4_dbg(dev, "%s: delete counter index %d for native device (%d) port %d\n"
						 , __func__, idx, slave, port);
					mlx4_bitmap_free(&priv->counters_table.bitmap, idx, MLX4_USE_RR);
					goto out;
				} else {
					mlx4_dbg(dev, "%s: can't delete default counter index %d for native device (%d) port %d\n"
						 , __func__, idx, slave, port);
					goto out;
				}
			}
			first++;
		}
		mlx4_dbg(dev, "%s: can't delete counter index %d for native device (%d) port %d\n"
			 , __func__, idx, slave, port);
	} else {
		first = 0;
		list_for_each_entry_safe(vf, tmp_vf,
					 &priv->counters_table.vf_list[slave - 1][port - 1],
					 list) {
			/* the first element is reserved */
			if (vf->index == idx) {
				/* clear the counter statistic */
				if (__mlx4_clear_if_stat(dev, vf->index))
					mlx4_dbg(dev, "%s: reset counter %d failed\n",
						 __func__, vf->index);
				if (first) {
					list_del(&vf->list);
					kfree(vf);
					mlx4_dbg(dev, "%s: delete counter index %d for slave %d port %d\n",
						 __func__, idx, slave, port);
					mlx4_bitmap_free(&priv->counters_table.bitmap, idx, MLX4_USE_RR);
					goto out;
				} else {
					mlx4_dbg(dev, "%s: can't delete default slave (%d) counter index %d for port %d\n"
						 , __func__, slave, idx, port);
					goto out;
				}
			}
			first++;
		}
		mlx4_dbg(dev, "%s: can't delete slave (%d) counter index %d for port %d\n"
			 , __func__, slave, idx, port);
	}

out:
	mutex_unlock(&priv->counters_table.mutex);
}

void mlx4_counter_free(struct mlx4_dev *dev, u8 port, u32 idx)
{
	u64 in_param = 0;
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct counter_index *counter, *tmp_counter;
	int first = 0;

	if (mlx4_is_mfunc(dev)) {
		set_param_l(&in_param, idx);
		mlx4_cmd(dev, in_param,
			 ((u32) port) << 8 | (u32) RES_COUNTER,
			 RES_OP_RESERVE,
			 MLX4_CMD_FREE_RES, MLX4_CMD_TIME_CLASS_A,
			 MLX4_CMD_WRAPPED);

		if (mlx4_is_slave(dev) && idx != MLX4_SINK_COUNTER_INDEX) {
			mutex_lock(&priv->counters_table.mutex);
			list_for_each_entry_safe(counter, tmp_counter,
						 &priv->counters_table.global_port_list[port - 1],
						 list) {
				if (counter->index == idx && first++) {
					list_del(&counter->list);
					kfree(counter);
					mlx4_dbg(dev, "%s: delete counter index %d for port %d\n"
						 , __func__, idx, port);
					mutex_unlock(&priv->counters_table.mutex);
					return;
				}
			}
			mutex_unlock(&priv->counters_table.mutex);
		}

		return;
	}
	__mlx4_counter_free(dev, 0, port, idx);
}
EXPORT_SYMBOL_GPL(mlx4_counter_free);

int __mlx4_clear_if_stat(struct mlx4_dev *dev,
			 u8 counter_index)
{
	struct mlx4_cmd_mailbox *if_stat_mailbox = NULL;
	int err = 0;
	u32 if_stat_in_mod = (counter_index & 0xff) | (1 << 31);

	if (counter_index == MLX4_SINK_COUNTER_INDEX)
		return -EINVAL;

	if (mlx4_is_slave(dev))
		return 0;

	if_stat_mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(if_stat_mailbox)) {
		err = PTR_ERR(if_stat_mailbox);
		return err;
	}

	err = mlx4_cmd_box(dev, 0, if_stat_mailbox->dma, if_stat_in_mod, 0,
			   MLX4_CMD_QUERY_IF_STAT, MLX4_CMD_TIME_CLASS_C,
			   MLX4_CMD_NATIVE);

	mlx4_free_cmd_mailbox(dev, if_stat_mailbox);
	return err;
}

u8 mlx4_get_default_counter_index(struct mlx4_dev *dev, int slave, int port)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct counter_index *new_counter_index;

	mutex_lock(&priv->counters_table.mutex);
	if (slave == 0) {
		new_counter_index = list_entry(priv->counters_table.global_port_list[port - 1].next,
					       struct counter_index,
					       list);
	} else {
		new_counter_index = list_entry(priv->counters_table.vf_list[slave - 1][port - 1].next,
					       struct counter_index,
					       list);
	}
	mutex_unlock(&priv->counters_table.mutex);

	mlx4_dbg(dev, "%s: return counter index %d for slave %d port %d\n",
		 __func__, new_counter_index->index, slave, port);


	return (u8)new_counter_index->index;
}

int mlx4_get_vport_ethtool_stats(struct mlx4_dev *dev, int port,
			 struct mlx4_en_vport_stats *vport_stats,
			 int reset)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_cmd_mailbox *if_stat_mailbox = NULL;
	union  mlx4_counter *counter;
	int err = 0;
	u32 if_stat_in_mod;
	struct counter_index *vport, *tmp_vport;

	if (!vport_stats)
		return -EINVAL;

	if_stat_mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(if_stat_mailbox)) {
		err = PTR_ERR(if_stat_mailbox);
		return err;
	}

	mutex_lock(&priv->counters_table.mutex);
	list_for_each_entry_safe(vport, tmp_vport,
				 &priv->counters_table.global_port_list[port - 1],
				 list) {
		if (vport->index == MLX4_SINK_COUNTER_INDEX)
			continue;

		memset(if_stat_mailbox->buf, 0, sizeof(union  mlx4_counter));
		if_stat_in_mod = (vport->index & 0xff) | ((reset & 1) << 31);
		err = mlx4_cmd_box(dev, 0, if_stat_mailbox->dma,
				   if_stat_in_mod, 0,
				   MLX4_CMD_QUERY_IF_STAT,
				   MLX4_CMD_TIME_CLASS_C,
				   MLX4_CMD_NATIVE);
		if (err) {
			mlx4_dbg(dev, "%s: failed to read statistics for counter index %d\n",
				 __func__, vport->index);
			goto if_stat_out;
		}
		counter = (union mlx4_counter *)if_stat_mailbox->buf;
		if ((counter->control.cnt_mode & 0xf) == 1) {
			vport_stats->rx_broadcast_packets += be64_to_cpu(counter->ext.counters[0].IfRxBroadcastFrames);
			vport_stats->rx_unicast_packets += be64_to_cpu(counter->ext.counters[0].IfRxUnicastFrames);
			vport_stats->rx_multicast_packets += be64_to_cpu(counter->ext.counters[0].IfRxMulticastFrames);
			vport_stats->tx_broadcast_packets += be64_to_cpu(counter->ext.counters[0].IfTxBroadcastFrames);
			vport_stats->tx_unicast_packets += be64_to_cpu(counter->ext.counters[0].IfTxUnicastFrames);
			vport_stats->tx_multicast_packets += be64_to_cpu(counter->ext.counters[0].IfTxMulticastFrames);
			vport_stats->rx_broadcast_bytes += be64_to_cpu(counter->ext.counters[0].IfRxBroadcastOctets);
			vport_stats->rx_unicast_bytes += be64_to_cpu(counter->ext.counters[0].IfRxUnicastOctets);
			vport_stats->rx_multicast_bytes += be64_to_cpu(counter->ext.counters[0].IfRxMulticastOctets);
			vport_stats->tx_broadcast_bytes += be64_to_cpu(counter->ext.counters[0].IfTxBroadcastOctets);
			vport_stats->tx_unicast_bytes += be64_to_cpu(counter->ext.counters[0].IfTxUnicastOctets);
			vport_stats->tx_multicast_bytes += be64_to_cpu(counter->ext.counters[0].IfTxMulticastOctets);
			vport_stats->rx_filtered += be64_to_cpu(counter->ext.counters[0].IfRxErrorFrames);
			vport_stats->rx_dropped += be64_to_cpu(counter->ext.counters[0].IfRxNoBufferFrames);
			vport_stats->tx_errors += be64_to_cpu(counter->ext.counters[0].IfTxDroppedFrames);
		}
	}

if_stat_out:
	mutex_unlock(&priv->counters_table.mutex);
	mlx4_free_cmd_mailbox(dev, if_stat_mailbox);

	return err;
}
EXPORT_SYMBOL_GPL(mlx4_get_vport_ethtool_stats);

int mlx4_get_vport_statistics(struct mlx4_dev *dev, int port,
			      struct net_device_stats *stats, int reset)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_cmd_mailbox *if_stat_mailbox = NULL;
	union  mlx4_counter *counter;
	int err = -EINVAL;
	u32 if_stat_in_mod;
	struct counter_index *vport, *tmp_vport;

	if (!stats)
		return -EINVAL;

	if_stat_mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(if_stat_mailbox)) {
		err = PTR_ERR(if_stat_mailbox);
		return err;
	}

	mutex_lock(&priv->counters_table.mutex);
	list_for_each_entry_safe(vport, tmp_vport,
				 &priv->counters_table.global_port_list[port - 1],
				 list) {
		if (vport->index == MLX4_SINK_COUNTER_INDEX)
			continue;

		memset(if_stat_mailbox->buf, 0, sizeof(union  mlx4_counter));
		if_stat_in_mod = (vport->index & 0xff) | ((reset & 1) << 31);
		err = mlx4_cmd_box(dev, 0, if_stat_mailbox->dma,
				   if_stat_in_mod, 0,
				   MLX4_CMD_QUERY_IF_STAT,
				   MLX4_CMD_TIME_CLASS_C,
				   MLX4_CMD_NATIVE);
		if (err) {
			mlx4_dbg(dev, "%s: failed to read statistics for counter index %d\n",
				 __func__, vport->index);
			goto if_stat_out;
		}
		counter = (union mlx4_counter *)if_stat_mailbox->buf;
		if ((counter->control.cnt_mode & 0xf) == 1) {
			stats->rx_packets += be64_to_cpu(counter->ext.counters[0].IfRxBroadcastFrames) +
				be64_to_cpu(counter->ext.counters[0].IfRxUnicastFrames) +
				be64_to_cpu(counter->ext.counters[0].IfRxMulticastFrames);
			stats->tx_packets += be64_to_cpu(counter->ext.counters[0].IfTxBroadcastFrames) +
				be64_to_cpu(counter->ext.counters[0].IfTxUnicastFrames) +
				be64_to_cpu(counter->ext.counters[0].IfTxMulticastFrames);
			stats->rx_bytes += be64_to_cpu(counter->ext.counters[0].IfRxBroadcastOctets) +
				be64_to_cpu(counter->ext.counters[0].IfRxUnicastOctets) +
				be64_to_cpu(counter->ext.counters[0].IfRxMulticastOctets);
			stats->tx_bytes += be64_to_cpu(counter->ext.counters[0].IfTxBroadcastOctets) +
				be64_to_cpu(counter->ext.counters[0].IfTxUnicastOctets) +
				be64_to_cpu(counter->ext.counters[0].IfTxMulticastOctets);
			/* PF&VFs are not expected to report errors in ifconfig.
			 * rx_errors will be reprted in PF's ethtool statistics,
			 * see: mlx4_en_DUMP_ETH_STATS
			 */
			stats->rx_errors = 0;
			stats->rx_dropped += be64_to_cpu(counter->ext.counters[0].IfRxNoBufferFrames);
			stats->tx_dropped += be64_to_cpu(counter->ext.counters[0].IfTxDroppedFrames);
			stats->multicast += be64_to_cpu(counter->ext.counters[0].IfTxMulticastFrames) +
				be64_to_cpu(counter->ext.counters[0].IfRxMulticastFrames);
		}
	}

if_stat_out:
	mutex_unlock(&priv->counters_table.mutex);
	mlx4_free_cmd_mailbox(dev, if_stat_mailbox);

	return err;
}
EXPORT_SYMBOL_GPL(mlx4_get_vport_statistics);

static int mlx4_setup_hca(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err;
	int port;
	__be32 ib_port_default_caps;

	err = mlx4_init_uar_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "user access region table (err=%d), aborting.\n",
			 err);
		return err;
	}

	err = mlx4_uar_alloc(dev, &priv->driver_uar);
	if (err) {
		mlx4_err(dev, "Failed to allocate driver access region "
			 "(err=%d), aborting.\n", err);
		goto err_uar_table_free;
	}

	priv->kar = ioremap((phys_addr_t) priv->driver_uar.pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!priv->kar) {
		mlx4_err(dev, "Couldn't map kernel access region, "
			 "aborting.\n");
		err = -ENOMEM;
		goto err_uar_free;
	}

	err = mlx4_init_pd_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "protection domain table (err=%d), aborting.\n", err);
		goto err_kar_unmap;
	}

	err = mlx4_init_xrcd_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "reliable connection domain table (err=%d), "
			 "aborting.\n", err);
		goto err_pd_table_free;
	}

	err = mlx4_init_mr_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "memory region table (err=%d), aborting.\n", err);
		goto err_xrcd_table_free;
	}

	if (!mlx4_is_slave(dev)) {
		err = mlx4_init_mcg_table(dev);
		if (err) {
			mlx4_err(dev, "Failed to initialize "
				 "multicast group table (err=%d), aborting.\n",
				 err);
			goto err_mr_table_free;
		}
		err = mlx4_config_mad_demux(dev);
		if (err) {
			mlx4_err(dev, "Failed in config_mad_demux\n");
			goto err_mr_table_free;
		}
	}

	err = mlx4_init_eq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "event queue table (err=%d), aborting.\n", err);
		goto err_mcg_table_free;
	}

	err = mlx4_cmd_use_events(dev);
	if (err) {
		mlx4_err(dev, "Failed to switch to event-driven "
			 "firmware commands (err=%d), aborting.\n", err);
		goto err_eq_table_free;
	}

	err = mlx4_NOP(dev);
	if (err) {
		if (dev->flags & MLX4_FLAG_MSI_X) {
			mlx4_warn(dev, "NOP command failed to generate MSI-X "
				  "interrupt IRQ %d).\n",
				  priv->eq_table.eq[dev->caps.num_comp_vectors].irq);
			mlx4_warn(dev, "Trying again without MSI-X.\n");
		} else {
			mlx4_err(dev, "NOP command failed to generate interrupt "
				 "(IRQ %d), aborting.\n",
				 priv->eq_table.eq[dev->caps.num_comp_vectors].irq);
			mlx4_err(dev, "BIOS or ACPI interrupt routing problem?\n");
		}

		goto err_cmd_poll;
	}

	mlx4_dbg(dev, "NOP command IRQ test passed\n");

	err = mlx4_init_cq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "completion queue table (err=%d), aborting.\n", err);
		goto err_cmd_poll;
	}

	err = mlx4_init_srq_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "shared receive queue table (err=%d), aborting.\n",
			 err);
		goto err_cq_table_free;
	}

	err = mlx4_init_qp_table(dev);
	if (err) {
		mlx4_err(dev, "Failed to initialize "
			 "queue pair table (err=%d), aborting.\n", err);
		goto err_srq_table_free;
	}

	if (!mlx4_is_slave(dev)) {
		for (port = 1; port <= dev->caps.num_ports; port++) {
			ib_port_default_caps = 0;
			err = mlx4_get_port_ib_caps(dev, port,
						    &ib_port_default_caps);
			if (err)
				mlx4_warn(dev, "failed to get port %d default "
					  "ib capabilities (%d). Continuing "
					  "with caps = 0\n", port, err);
			dev->caps.ib_port_def_cap[port] = ib_port_default_caps;

			/* initialize per-slave default ib port capabilities */
			if (mlx4_is_master(dev)) {
				int i;
				for (i = 0; i < dev->num_slaves; i++) {
					if (i == mlx4_master_func_num(dev))
						continue;
					priv->mfunc.master.slave_state[i].ib_cap_mask[port] =
							ib_port_default_caps;
				}
			}

			dev->caps.port_ib_mtu[port] = IB_MTU_4096;

			err = mlx4_SET_PORT(dev, port, mlx4_is_master(dev) ?
					    dev->caps.pkey_table_len[port] : -1);
			if (err) {
				mlx4_err(dev, "Failed to set port %d (err=%d), "
					 "aborting\n", port, err);
				goto err_qp_table_free;
			}
		}
	}

	return 0;

err_qp_table_free:
	mlx4_cleanup_qp_table(dev);

err_srq_table_free:
	mlx4_cleanup_srq_table(dev);

err_cq_table_free:
	mlx4_cleanup_cq_table(dev);

err_cmd_poll:
	mlx4_cmd_use_polling(dev);

err_eq_table_free:
	mlx4_cleanup_eq_table(dev);

err_mcg_table_free:
	if (!mlx4_is_slave(dev))
		mlx4_cleanup_mcg_table(dev);

err_mr_table_free:
	mlx4_cleanup_mr_table(dev);

err_xrcd_table_free:
	mlx4_cleanup_xrcd_table(dev);

err_pd_table_free:
	mlx4_cleanup_pd_table(dev);

err_kar_unmap:
	iounmap(priv->kar);

err_uar_free:
	mlx4_uar_free(dev, &priv->driver_uar);

err_uar_table_free:
	mlx4_cleanup_uar_table(dev);
	return err;
}

static void mlx4_enable_msi_x(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct msix_entry *entries;
	int nreq;
	int err;
	int i;

	if (msi_x) {
		nreq = dev->caps.num_ports * num_online_cpus() + MSIX_LEGACY_SZ;

		nreq = min_t(int, dev->caps.num_eqs - dev->caps.reserved_eqs,
			     nreq);
#ifdef CONFIG_PPC
		nreq = min_t(int, nreq, PPC_MAX_MSIX);
#endif

		if (msi_x > 1)
			nreq = min_t(int, nreq, msi_x);

		entries = kcalloc(nreq, sizeof *entries, GFP_KERNEL);
		if (!entries)
			goto no_msi;

		for (i = 0; i < nreq; ++i)
			entries[i].entry = i;

	retry:
		err = pci_enable_msix(dev->pdev, entries, nreq);
		if (err) {
			/* Try again if at least 2 vectors are available */
			if (err > 1) {
				mlx4_info(dev, "Requested %d vectors, "
					  "but only %d MSI-X vectors available, "
					  "trying again\n", nreq, err);
				nreq = err;
				goto retry;
			}
			kfree(entries);
			goto no_msi;
		}

		if (nreq <
		    MSIX_LEGACY_SZ + dev->caps.num_ports * MIN_MSIX_P_PORT) {
			/*Working in legacy mode , all EQ's shared*/
			dev->caps.comp_pool           = 0;
			dev->caps.num_comp_vectors = nreq - 1;
		} else {
			dev->caps.comp_pool           = nreq - MSIX_LEGACY_SZ;
			dev->caps.num_comp_vectors = MSIX_LEGACY_SZ - 1;
		}
		for (i = 0; i < nreq; ++i)
			priv->eq_table.eq[i].irq = entries[i].vector;

		dev->flags |= MLX4_FLAG_MSI_X;

		kfree(entries);
		return;
	}

no_msi:
	dev->caps.num_comp_vectors = 1;
	dev->caps.comp_pool	   = 0;

	for (i = 0; i < 2; ++i)
		priv->eq_table.eq[i].irq = dev->pdev->irq;
}

static int mlx4_init_port_info(struct mlx4_dev *dev, int port)
{
	struct mlx4_port_info *info = &mlx4_priv(dev)->port[port];
	int err = 0;

	info->dev = dev;
	info->port = port;
	if (!mlx4_is_slave(dev)) {
		mlx4_init_mac_table(dev, &info->mac_table);
		mlx4_init_vlan_table(dev, &info->vlan_table);
		mlx4_init_roce_gid_table(dev, &info->gid_table);
		info->base_qpn = mlx4_get_base_qpn(dev, port);
	}

	sprintf(info->dev_name, "mlx4_port%d", port);
	info->port_attr.attr.name = info->dev_name;
	if (mlx4_is_mfunc(dev))
		info->port_attr.attr.mode = S_IRUGO;
	else {
		info->port_attr.attr.mode = S_IRUGO | S_IWUSR;
		info->port_attr.store     = set_port_type;
	}
	info->port_attr.show      = show_port_type;
	sysfs_attr_init(&info->port_attr.attr);

	err = device_create_file(&dev->pdev->dev, &info->port_attr);
	if (err) {
		mlx4_err(dev, "Failed to create file for port %d\n", port);
		info->port = -1;
	}

	sprintf(info->dev_mtu_name, "mlx4_port%d_mtu", port);
	info->port_mtu_attr.attr.name = info->dev_mtu_name;
	if (mlx4_is_mfunc(dev))
		info->port_mtu_attr.attr.mode = S_IRUGO;
	else {
		info->port_mtu_attr.attr.mode = S_IRUGO | S_IWUSR;
		info->port_mtu_attr.store     = set_port_ib_mtu;
	}
	info->port_mtu_attr.show      = show_port_ib_mtu;
	sysfs_attr_init(&info->port_mtu_attr.attr);

	err = device_create_file(&dev->pdev->dev, &info->port_mtu_attr);
	if (err) {
		mlx4_err(dev, "Failed to create mtu file for port %d\n", port);
		device_remove_file(&info->dev->pdev->dev, &info->port_attr);
		info->port = -1;
	}

	return err;
}

static void mlx4_cleanup_port_info(struct mlx4_port_info *info)
{
	if (info->port < 0)
		return;

	device_remove_file(&info->dev->pdev->dev, &info->port_attr);
	device_remove_file(&info->dev->pdev->dev, &info->port_mtu_attr);
}

static int mlx4_init_steering(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int num_entries = dev->caps.num_ports;
	int i, j;

	priv->steer = kzalloc(sizeof(struct mlx4_steer) * num_entries, GFP_KERNEL);
	if (!priv->steer)
		return -ENOMEM;

	for (i = 0; i < num_entries; i++)
		for (j = 0; j < MLX4_NUM_STEERS; j++) {
			INIT_LIST_HEAD(&priv->steer[i].promisc_qps[j]);
			INIT_LIST_HEAD(&priv->steer[i].steer_entries[j]);
		}
	return 0;
}

static void mlx4_clear_steering(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_steer_index *entry, *tmp_entry;
	struct mlx4_promisc_qp *pqp, *tmp_pqp;
	int num_entries = dev->caps.num_ports;
	int i, j;

	for (i = 0; i < num_entries; i++) {
		for (j = 0; j < MLX4_NUM_STEERS; j++) {
			list_for_each_entry_safe(pqp, tmp_pqp,
						 &priv->steer[i].promisc_qps[j],
						 list) {
				list_del(&pqp->list);
				kfree(pqp);
			}
			list_for_each_entry_safe(entry, tmp_entry,
						 &priv->steer[i].steer_entries[j],
						 list) {
				list_del(&entry->list);
				list_for_each_entry_safe(pqp, tmp_pqp,
							 &entry->duplicates,
							 list) {
					list_del(&pqp->list);
					kfree(pqp);
				}
				kfree(entry);
			}
		}
	}
	kfree(priv->steer);
}

static int extended_func_num(struct pci_dev *pdev)
{
	return PCI_SLOT(pdev->devfn) * 8 + PCI_FUNC(pdev->devfn);
}

#define MLX4_OWNER_BASE	0x8069c
#define MLX4_OWNER_SIZE	4

static int mlx4_get_ownership(struct mlx4_dev *dev)
{
	void __iomem *owner;
	u32 ret;

	if (pci_channel_offline(dev->pdev))
		return -EIO;

	owner = ioremap(pci_resource_start(dev->pdev, 0) + MLX4_OWNER_BASE,
			MLX4_OWNER_SIZE);
	if (!owner) {
		mlx4_err(dev, "Failed to obtain ownership bit\n");
		return -ENOMEM;
	}

	ret = readl(owner);
	iounmap(owner);
	return (int) !!ret;
}

static void mlx4_free_ownership(struct mlx4_dev *dev)
{
	void __iomem *owner;

	if (pci_channel_offline(dev->pdev))
		return;

	owner = ioremap(pci_resource_start(dev->pdev, 0) + MLX4_OWNER_BASE,
			MLX4_OWNER_SIZE);
	if (!owner) {
		mlx4_err(dev, "Failed to obtain ownership bit\n");
		return;
	}
	writel(0, owner);
	msleep(1000);
	iounmap(owner);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
static int mlx4_find_vfs(struct pci_dev *pdev)
{
	struct pci_dev *dev;
	int vfs = 0, pos;
	u16 offset, stride;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return 0;
	pci_read_config_word(pdev, pos + PCI_SRIOV_VF_OFFSET, &offset);
	pci_read_config_word(pdev, pos + PCI_SRIOV_VF_STRIDE, &stride);

	dev = pci_get_device(pdev->vendor, PCI_ANY_ID, NULL);
	while (dev) {
		if (dev->is_virtfn && pci_physfn(dev) == pdev) {
			vfs++;
		}
		dev = pci_get_device(pdev->vendor, PCI_ANY_ID, dev);
	}
	return vfs;
}
#endif

static int mlx4_load_one(struct pci_dev *pdev, int pci_dev_data,
			 int total_vfs, int *nvfs, struct mlx4_priv *priv,
			 int reset_flow)
{
	struct mlx4_dev *dev;
	int read_num_eqs = 1;
	int num_sys_eqs = 0;
	unsigned sum = 0;
	int err;
	int port;
	int i;
	struct mlx4_dev_cap *dev_cap = NULL;
	int num_vfs_argc =
		mlx4_get_argc(num_vfs.dbdf2val.tbl, pci_physfn(pdev));
	int probe_vfs_argc =
		mlx4_get_argc(probe_vf.dbdf2val.tbl, pci_physfn(pdev));
	/* pre_vfs will contain the number of VFs which were active when
	remove_one was invoked on the PF driver. In this case,
	the PF driver did not disable SRIOV during remove_one.
	When the PF is reloaded (mlx4_load_one), SRIOV is therefore
	still enabled, and pci_enable_sriov should not be called. */
	int pre_vfs = 0;

	dev = &priv->dev;

	INIT_LIST_HEAD(&priv->dev_list);
	INIT_LIST_HEAD(&priv->ctx_list);
	spin_lock_init(&priv->ctx_lock);

	mutex_init(&priv->port_mutex);

	INIT_LIST_HEAD(&priv->pgdir_list);
	mutex_init(&priv->pgdir_mutex);

	INIT_LIST_HEAD(&priv->bf_list);
	mutex_init(&priv->bf_mutex);

	dev->rev_id = pdev->revision;
	dev->numa_node = dev_to_node(&pdev->dev);
	if (dev->numa_node == -1)
		dev->numa_node = first_online_node;
	memcpy(dev->nvfs, nvfs, sizeof(dev->nvfs));

	if (pci_dev_data & MLX4_PCI_DEV_IS_VF) {
		mlx4_warn(dev, "Detected virtual function - running in slave mode\n");
		dev->flags |= MLX4_FLAG_SLAVE;
	} else {
		/* We reset the device and enable SRIOV only for physical
		 * devices.  Try to claim ownership on the device;
		 * if already taken, skip -- do not allow multiple PFs */
		err = mlx4_get_ownership(dev);
		if (err) {
			if (err < 0)
				goto err_end;
			else {
				mlx4_warn(dev, "Multiple PFs not yet supported."
					  " Skipping PF.\n");
				err = -EINVAL;
				goto err_end;
			}
		}

		atomic_set(&priv->opreq_count, 0);
		INIT_WORK(&priv->opreq_task, mlx4_opreq_action);

		/*
		 * Now reset the HCA before we touch the PCI capabilities or
		 * attempt a firmware command, since a boot ROM may have left
		 * the HCA in an undefined state.
		 */
		err = mlx4_reset(dev);
		if (err) {
			mlx4_err(dev, "Failed to reset HCA, aborting.\n");
			goto err_ownership;
		}

		if (total_vfs) {
			dev->flags = MLX4_FLAG_MASTER | MLX4_FLAG_SRIOV;
			dev->num_vfs = total_vfs;
		}
	}

	/* on load removing any previous indication of internal error, device is up */
	dev->state = MLX4_DEVICE_STATE_UP;

slave_start:
	err = mlx4_cmd_init(dev);
	if (err) {
		mlx4_err(dev, "Failed to init command interface, aborting.\n");
		goto err_ownership;
	}

	/* In slave functions, the communication channel must be initialized
	 * before posting commands. Also, init num_slaves before calling
	 * mlx4_init_hca */
	if (mlx4_is_mfunc(dev)) {
		if (mlx4_is_master(dev))
			dev->num_slaves = MLX4_MAX_NUM_SLAVES;
		else {
			dev->num_slaves = 0;
			err = mlx4_multi_func_init(dev);
			if (err) {
				mlx4_err(dev, "Failed to init slave mfunc"
					 " interface, aborting.\n");
				goto err_cmd;
			}
		}
	}

	err = mlx4_init_fw(dev);
	if (err) {
		mlx4_err(dev, "Failed to init fw, aborting.\n");
		goto err_mfunc;
	}

	if (mlx4_is_master(dev)) {
		dev_cap = kzalloc(sizeof(*dev_cap), GFP_KERNEL);
		if (!dev_cap) {
			mlx4_err(dev, "Failed to allocate memory for dev_cap\n");
			err = -ENOMEM;
			goto err_mfunc;
		}

		err = mlx4_QUERY_DEV_CAP(dev, dev_cap);
		if (err) {
			mlx4_err(dev, "QUERY_DEV_CAP command failed, aborting.\n");
			goto err_mfunc;
		}

		/* Checking for 64 VFs as a limitation of CX2 */
		if (!(dev_cap->flags2 & MLX4_DEV_CAP_FLAG2_80_VFS) &&
		    nvfs[0] + nvfs[1] + nvfs[2] >= 64) {
			mlx4_err(dev, "Required more than 64 VFs, but the FW "
				      "doesn't support such a number\n");
			goto err_mfunc;
		}

		if (read_num_eqs) {
			num_sys_eqs = dev_cap->num_sys_eqs;
			/* working in legacy mode */
			if (!num_sys_eqs) {
				mlx4_warn(dev, "Enabling SR-IOV with %d VFs\n",
					  total_vfs);
				dev->dev_vfs = kzalloc(
						total_vfs * sizeof(*dev->dev_vfs),
						GFP_KERNEL);
				if (NULL == dev->dev_vfs) {
					mlx4_err(dev, "Failed to allocate memory for VFs\n");
					dev->flags &= ~(MLX4_FLAG_MASTER);
					dev->flags &= ~(MLX4_FLAG_SRIOV);
					dev->num_vfs = 0;
					err = 0;
					if (reset_flow) {
						err = -ENOMEM;
						goto err_mfunc;
					}
				} else if (!reset_flow) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
					pre_vfs = mlx4_find_vfs(pdev);
#else
					pre_vfs = pci_num_vf(pdev);
#endif
					if (!pre_vfs) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
						atomic_inc(&pf_loading);
						err = pci_enable_sriov(pdev, total_vfs);
						atomic_dec(&pf_loading);
#else
    					err = pci_enable_sriov(pdev, total_vfs);
#endif
					} else {
						/* continue without enable sriov (it was enabled) */
						err = 0;
					}
					if (err) {
						mlx4_err(dev, "Failed to enable SR-IOV, continuing without SR-IOV (err = %d).\n",
							 err);
						dev->flags &= ~(MLX4_FLAG_MASTER);
						dev->flags &= ~(MLX4_FLAG_SRIOV);
						dev->num_vfs = 0;
						err = 0;
					} else {
						mlx4_warn(dev, "Running in master mode\n");
					}
				}

				mlx4_cmd_cleanup(dev);
				err = mlx4_reset(dev);
				if (err) {
					mlx4_err(dev, "Failed to reset HCA, aborting.\n");
					goto err_sriov;
				}
				read_num_eqs = 0;
				kfree(dev_cap);
				dev_cap = NULL;
				goto slave_start;
			}
		}
		kfree(dev_cap);
		dev_cap = NULL;
	}
	err = mlx4_init_hca(dev);
	if (err) {
		if (err == -EACCES) {
			/* Not primary Physical function
			 * Running in slave mode */
			mlx4_cmd_cleanup(dev);
			dev->flags |= MLX4_FLAG_SLAVE;
			dev->flags &= ~MLX4_FLAG_MASTER;
			goto slave_start;
		} else
			goto err_mfunc;
	}

	if (mlx4_is_master(dev) && num_sys_eqs) {
		mlx4_warn(dev, "Enabling SR-IOV with %d VFs\n", total_vfs);
		dev->dev_vfs = kzalloc(total_vfs * sizeof(*dev->dev_vfs),
				       GFP_KERNEL);
		if (NULL == dev->dev_vfs) {
			mlx4_err(dev, "Failed to allocate memory for VFs\n");
			dev->flags &= ~(MLX4_FLAG_MASTER);
			dev->flags &= ~(MLX4_FLAG_SRIOV);
			dev->num_vfs = 0;
			err = 0;
			if (reset_flow) {
				err = -ENOMEM;
				goto err_close;
			}
		} else if (!reset_flow) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
			pre_vfs = mlx4_find_vfs(pdev);
#else
			pre_vfs = pci_num_vf(pdev);
#endif
			if (!pre_vfs) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
				atomic_inc(&pf_loading);
				err = pci_enable_sriov(pdev, total_vfs);
				atomic_dec(&pf_loading);
#else
				err = pci_enable_sriov(pdev, total_vfs);
#endif
			} else {
				err = 0;
			}
			if (err) {
				mlx4_err(dev, "Failed to enable SR-IOV, continuing without SR-IOV (err = %d).\n",
					 err);
				dev->flags &= ~(MLX4_FLAG_MASTER);
				dev->flags &= ~(MLX4_FLAG_SRIOV);
				dev->num_vfs = 0;
				err = 0;
			} else {
				mlx4_warn(dev, "Running in master mode\n");
			}
		}
	}

	if (!reset_flow && mlx4_is_master(dev)) {
		int ib_ports = 0;
		for (i = 0; i < dev->caps.num_ports; ++i)
			ib_ports +=
				(dev->caps.port_type[i] ==
				 MLX4_PORT_TYPE_IB);
		if (ib_ports &&
		    (num_vfs_argc > 1 || probe_vfs_argc > 1)) {
			err = -EINVAL;
			mlx4_err(dev,
				 "Invalid syntax of num_vfs/probe_vfs "
				 "with IB port\n");
			goto err_close;
		}
		if (dev->nvfs[1] && dev->caps.num_ports <= 1) {
			err = -EINVAL;
			mlx4_err(dev,
				 "Error: Trying to configure %d VFs on port 2,"
				 " but HCA has only %d physical ports\n",
				 dev->nvfs[1], dev->caps.num_ports);
			goto err_close;
		}
	}

	if (mlx4_is_master(dev)) {
		for (i = 0; i < sizeof(dev->nvfs)/sizeof(dev->nvfs[0]); i++) {
			unsigned j;
			for (j = 0; j < dev->nvfs[i]; ++sum, ++j) {
				dev->dev_vfs[sum].min_port = i < 2 ? i + 1 : 1;
				dev->dev_vfs[sum].n_ports = i < 2 ? 1 :
					dev->caps.num_ports;
			}
		}
	}

	err = mlx4_init_counters_table(dev);
	if (err && err != -ENOENT) {
		mlx4_err(dev, "Failed to initialize counters table (err=%d), "
			 "aborting.\n", err);
		goto err_close;
	}

	if (mlx4_is_master(dev)) {
		err = mlx4_cmd_init_master(dev);
		if (err) {
			mlx4_err(dev, "Failed to enable SR-IOV aborting\n");
			goto err_counters;
		}

		/* In master functions, the communication channel
		 * must be initialized after obtaining its address from fw
		 */
		err = mlx4_multi_func_init(dev);
		if (err) {
			mlx4_err(dev, "Failed to init master mfunc interface, aborting.\n");
			goto err_master_cmd;
		}
	}

	err = mlx4_alloc_eq_table(dev);
	if (err)
		goto err_master_mfunc;

	priv->msix_ctl.pool_bm = 0;
	mutex_init(&priv->msix_ctl.pool_lock);

	mlx4_enable_msi_x(dev);
	if ((mlx4_is_mfunc(dev)) &&
	    !(dev->flags & MLX4_FLAG_MSI_X)) {
		err = -ENOSYS;
		mlx4_err(dev, "INTx is not supported in multi-function mode."
			 " aborting.\n");
		goto err_free_eq;
	}

	if (!mlx4_is_slave(dev)) {
		err = mlx4_init_steering(dev);
		if (err)
			goto err_msix;

		if (dev->caps.roce_mode == MLX4_ROCE_MODE_2) {
			err = mlx4_config_rocev2_udp_port(dev, dev->caps.roce_proto_config);

			if (err)
				goto err_steer;
		}
	}

	err = mlx4_setup_hca(dev);
	if (err == -EBUSY && (dev->flags & MLX4_FLAG_MSI_X) &&
	    !mlx4_is_mfunc(dev)) {
		dev->flags &= ~MLX4_FLAG_MSI_X;
		dev->caps.num_comp_vectors = 1;
		dev->caps.comp_pool	   = 0;
		pci_disable_msix(pdev);
		err = mlx4_setup_hca(dev);
	}
	if (err)
		goto err_steer;

	mlx4_init_quotas(dev);
	/* Once PF resources are ready arm its comm channel to enable getting commands */
	if (mlx4_is_master(dev)) {
		err = mlx4_ARM_COMM_CHANNEL(dev);
		if (err) {
			mlx4_err(dev, " Failed to arm comm channel eq: %x\n", err);
			goto err_steer;
		}
	}

	for (port = 1; port <= dev->caps.num_ports; port++) {
		err = mlx4_init_port_info(dev, port);
		if (err)
			goto err_port;
	}

	err = mlx4_register_device(dev);
	if (err)
		goto err_port;

	mlx4_request_modules(dev);

	mlx4_sense_init(dev);
	mlx4_start_sense(dev);

	priv->pci_dev_data = pci_dev_data;
	pci_set_drvdata(pdev, dev);

	return 0;

err_port:
	for (--port; port >= 1; --port)
		mlx4_cleanup_port_info(&priv->port[port]);

	mlx4_cleanup_qp_table(dev);
	mlx4_cleanup_srq_table(dev);
	mlx4_cleanup_cq_table(dev);
	mlx4_cmd_use_polling(dev);
	mlx4_cleanup_eq_table(dev);
	mlx4_cleanup_mcg_table(dev);
	mlx4_cleanup_mr_table(dev);
	mlx4_cleanup_xrcd_table(dev);
	mlx4_cleanup_pd_table(dev);
	mlx4_cleanup_uar_table(dev);

err_steer:
	if (!mlx4_is_slave(dev))
		mlx4_clear_steering(dev);
err_msix:
	if (dev->flags & MLX4_FLAG_MSI_X)
		pci_disable_msix(pdev);

err_free_eq:
	mlx4_free_eq_table(dev);

err_master_mfunc:
	if (mlx4_is_master(dev)) {
		mlx4_free_resource_tracker(dev, RES_TR_FREE_STRUCTS_ONLY);
		mlx4_multi_func_cleanup(dev);
	}

	if (mlx4_is_slave(dev)) {
		kfree(dev->caps.qp0_qkey);
		kfree(dev->caps.qp0_tunnel);
		kfree(dev->caps.qp0_proxy);
		kfree(dev->caps.qp1_tunnel);
		kfree(dev->caps.qp1_proxy);
	}

err_master_cmd:
	if (mlx4_is_master(dev))
		mlx4_cmd_cleanup_master(dev);

err_counters:
	mlx4_cleanup_counters_table(dev);

err_close:
	mlx4_close_hca(dev);

err_mfunc:
	if (mlx4_is_slave(dev))
		mlx4_multi_func_cleanup(dev);

	if (mlx4_is_master(dev))
		kfree(dev_cap);

err_cmd:
	mlx4_cmd_cleanup(dev);

err_sriov:
	kfree(priv->dev.dev_vfs);

	if (dev->flags & MLX4_FLAG_SRIOV && !reset_flow)
		pci_disable_sriov(pdev);

err_ownership:
	if (!mlx4_is_slave(dev))
		mlx4_free_ownership(dev);

err_end:
	return err;
}

static int __mlx4_init_one(struct pci_dev *pdev, int pci_dev_data, struct mlx4_priv *priv)
{
	int err;
	unsigned int i;
	unsigned total_vfs = 0;
	int nvfs[MLX4_MAX_PORTS + 1] = {0, 0, 0};
	int prb_vf[MLX4_MAX_PORTS + 1] = {0, 0, 0};
	const int param_map[MLX4_MAX_PORTS + 1][MLX4_MAX_PORTS + 1] = {
		{2, 0, 0}, {0, 1, 2}, {0, 1, 2} };
	int num_vfs_argc =
		mlx4_get_argc(num_vfs.dbdf2val.tbl, pci_physfn(pdev));
	int probe_vfs_argc =
		mlx4_get_argc(probe_vf.dbdf2val.tbl, pci_physfn(pdev));

	pr_info(DRV_NAME ": Initializing %s\n", pci_name(pdev));

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device, aborting.\n");
		return err;
	}

	for (i = 0; i < num_vfs_argc;
	     total_vfs += nvfs[param_map[num_vfs_argc - 1][i]], i++) {
		int *cur_nvfs = &nvfs[param_map[num_vfs_argc - 1][i]];
		mlx4_get_val(num_vfs.dbdf2val.tbl, pci_physfn(pdev), i,
			     cur_nvfs);
		if (*cur_nvfs < 0) {
			dev_err(&pdev->dev, "num_vfs module parameter cannot be negative\n");
			return -EINVAL;
		}
	}
	for (i = 0; i < probe_vfs_argc; i++) {
		int *cur_prbvf = &prb_vf[param_map[probe_vfs_argc - 1][i]];
		mlx4_get_val(probe_vf.dbdf2val.tbl, pci_physfn(pdev), i,
			     cur_prbvf);
		if (*cur_prbvf < 0) {
			dev_err(&pdev->dev, "probe_vf module parameter cannot be negative\n");
			return -EINVAL;
		}
	}
	for (i = 0; i < sizeof(nvfs)/sizeof(nvfs[0]); i++) {
		if (prb_vf[i] > nvfs[i]) {
			dev_err(&pdev->dev, "probe_vf module parameter cannot be greater than num_vfs\n");
			return -EINVAL;
		}
	}
	if (total_vfs > MLX4_MAX_NUM_VF) {
		dev_err(&pdev->dev, "total vfs (%d) can't be more than %d\n",
			total_vfs, MLX4_MAX_NUM_VF);
		return -EINVAL;
	}

	for (i = 0; i < MLX4_MAX_PORTS; i++) {
		if (nvfs[i] + nvfs[2] >= MLX4_MAX_NUM_VF_P_PORT) {
			dev_err(&pdev->dev,
				"Requested more VF's (%d) for port (%d) than allowed (%d)\n",
				nvfs[i] + nvfs[2], i + 1,
				MLX4_MAX_NUM_VF_P_PORT - 1);
			return -EINVAL;
		}
	}

	/* Check for BARs.
	 */
	if (!(pci_dev_data & MLX4_PCI_DEV_IS_VF) &&
	    !(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev,
			"Missing DCS, aborting. (driver_data: 0x%x, pci_resource_flags(pdev, 0):0x%lx)\n",
			pci_dev_data, pci_resource_flags(pdev, 0));
		err = -ENODEV;
		goto err_disable_pdev;
	}
	if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "Missing UAR, aborting.\n");
		err = -ENODEV;
		goto err_disable_pdev;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		dev_err(&pdev->dev, "Couldn't get PCI resources, aborting\n");
		goto err_disable_pdev;
	}

	pci_set_master(pdev);

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit PCI DMA mask.\n");
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set PCI DMA mask, aborting.\n");
			goto err_release_regions;
		}
	}
	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev,
			 "Warning: couldn't set 64-bit consistent PCI DMA mask.\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev,
				"Can't set consistent PCI DMA mask,aborting.\n");
			goto err_release_regions;
		}
	}

	/* Allow large DMA segments, up to the firmware limit of 1 GB */
	dma_set_max_seg_size(&pdev->dev, 1024 * 1024 * 1024);

	/* Detect if this device is a virtual function */
	if (pci_dev_data & MLX4_PCI_DEV_IS_VF) {
		if (total_vfs) {
			/* When acting as pf, we normally skip vfs unless     */
			/* explicitly requested to probe them.		      */
			unsigned vfs_offset = 0;
			/* Calculate the total number of vfs in the previous  */
			/* groups. For example, if the VF we're adding belongs*/
			/* to group (port 1+2),				      */
			/* vfs_offset = num_vfs_on_port1 + num_vfs_on_port2   */
			for (i = 0; i < sizeof(nvfs)/sizeof(nvfs[0]) &&
			     vfs_offset + nvfs[i] < extended_func_num(pdev);
			     vfs_offset += nvfs[i], i++)
				;
			if (i == sizeof(nvfs)/sizeof(nvfs[0])) {
				dev_err(&pdev->dev, "Internal error, invalid nvfs\n");
				err = -ENODEV;
				goto err_release_regions;
			}
			if ((extended_func_num(pdev) - vfs_offset)
			    > prb_vf[i]) {
				dev_warn(&pdev->dev, "Skipping virtual function:%d\n",
					 extended_func_num(pdev));
				err = -ENODEV;
				goto err_release_regions;
			}
		}
	}

	err = mlx4_catas_init(&priv->dev);
	if (err)
		goto err_release_regions;
	err = mlx4_load_one(pdev, pci_dev_data, total_vfs, nvfs, priv, 0);
	if (err)
		goto err_catas;
	return 0;

err_catas:
	mlx4_catas_end(&priv->dev);
err_release_regions:
	pci_release_regions(pdev);

err_disable_pdev:
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	return err;
}

static int __devinit mlx4_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct mlx4_priv *priv;
	struct mlx4_dev *dev;
	int ret;

	printk_once(KERN_INFO "%s", mlx4_version);
	priv = kzalloc(sizeof *priv, GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Device struct alloc failed, aborting.\n");
		return -ENOMEM;
	}

	dev = &priv->dev;
	dev->pdev = pdev;
	mutex_init(&dev->interface_state_mutex);
	mutex_init(&dev->device_state_mutex);
	ret = __mlx4_init_one(pdev, id->driver_data, priv);
	if (ret)
		kfree(priv);
	else
		/* saving state once prob was finished successfully */
		pci_save_state(pdev);
	return ret;
}

static void mlx4_clean_dev(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	size_t dev_persistent_end_offset = offsetof(struct mlx4_dev, flags);
	size_t priv_clean_size = sizeof(struct mlx4_priv) - dev_persistent_end_offset;

	memset((u8 *)priv + dev_persistent_end_offset, 0, priv_clean_size);
}

static void mlx4_unload_one(struct pci_dev *pdev, struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int p;
	int i;

	/* saving current ports type for further use */
	for (i = 0; i < dev->caps.num_ports; i++) {
		dev->curr_port_type[i] = dev->caps.port_type[i + 1];
		dev->curr_port_poss_type[i] = dev->caps.possible_type[i + 1];
	}

	mlx4_stop_sense(dev);
	mlx4_unregister_device(dev);

	for (p = 1; p <= dev->caps.num_ports; p++) {
		mlx4_cleanup_port_info(&priv->port[p]);
		mlx4_CLOSE_PORT(dev, p);
	}

	if (mlx4_is_master(dev))
		mlx4_free_resource_tracker(dev,
					   RES_TR_FREE_SLAVES_ONLY);

	mlx4_cleanup_qp_table(dev);
	mlx4_cleanup_srq_table(dev);
	mlx4_cleanup_cq_table(dev);
	mlx4_cmd_use_polling(dev);
	mlx4_cleanup_eq_table(dev);
	mlx4_cleanup_mcg_table(dev);
	mlx4_cleanup_mr_table(dev);
	mlx4_cleanup_xrcd_table(dev);
	mlx4_cleanup_pd_table(dev);
	mlx4_cleanup_counters_table(dev);

	if (mlx4_is_master(dev))
		mlx4_free_resource_tracker(dev,
					   RES_TR_FREE_STRUCTS_ONLY);

	iounmap(priv->kar);
	mlx4_uar_free(dev, &priv->driver_uar);
	mlx4_cleanup_uar_table(dev);
	if (!mlx4_is_slave(dev))
		mlx4_clear_steering(dev);
	mlx4_free_eq_table(dev);
	if (mlx4_is_master(dev))
		mlx4_multi_func_cleanup(dev);
	mlx4_close_hca(dev);
	if (mlx4_is_slave(dev))
		mlx4_multi_func_cleanup(dev);
	mlx4_cmd_cleanup(dev);
	if (mlx4_is_master(dev))
			mlx4_cmd_cleanup_master(dev);

	if (dev->flags & MLX4_FLAG_MSI_X)
		pci_disable_msix(pdev);

	if (!mlx4_is_slave(dev))
		mlx4_free_ownership(dev);

	kfree(dev->caps.qp0_qkey);
	kfree(dev->caps.qp0_tunnel);
	kfree(dev->caps.qp0_proxy);
	kfree(dev->caps.qp1_tunnel);
	kfree(dev->caps.qp1_proxy);
	kfree(dev->dev_vfs);
	mlx4_clean_dev(dev);
}

static void mlx4_remove_one(struct pci_dev *pdev)
{
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);
	int active_vfs = 0;

	if (dev) {
		/* in SRIOV it is not allowed to unload the pf's
		 * driver while there are alive vf's
		 */
		if (mlx4_is_master(dev)) {
			active_vfs = mlx4_how_many_lives_vf(dev);
			if (active_vfs)
				mlx4_err(dev, "Removing PF when there are assigned VF's !!!\n");
		}
		mutex_lock(&dev->interface_state_mutex);
		dev->interface_state |= MLX4_INTERFACE_STATE_DELETION;
		mutex_unlock(&dev->interface_state_mutex);
		/* device marked to be under deletion running now without the lock letting other tasks to be terminated */
		if (dev->interface_state & MLX4_INTERFACE_STATE_UP)
			mlx4_unload_one(pdev, dev);
		else
			mlx4_err(dev, "mlx4_remove_one, interface is down, NOP\n");
		mlx4_catas_end(dev);
		pci_set_drvdata(pdev, NULL);
		kfree(mlx4_priv(dev));
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
	if (mlx4_find_vfs(pdev)) {
#else
	if (pci_num_vf(pdev)) {
#endif
		if (!active_vfs) {
			dev_warn(&pdev->dev, "Disabling SR-IOV\n");
			pci_disable_sriov(pdev);
		}
	}

	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static int restore_current_port_types(struct mlx4_dev *dev,
				      enum mlx4_port_type *types,
				      enum mlx4_port_type *poss_types)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	int err, i;

	mlx4_stop_sense(dev);
	mutex_lock(&priv->port_mutex);
	for (i = 0; i < dev->caps.num_ports; i++)
		dev->caps.possible_type[i + 1] = poss_types[i];
	err = mlx4_change_port_types(dev, types);
	mlx4_start_sense(dev);
	mutex_unlock(&priv->port_mutex);
	return err;
}

int mlx4_restart_one(struct pci_dev *pdev)
{
	struct mlx4_dev	 *dev  = pci_get_drvdata(pdev);
	struct mlx4_priv *priv = mlx4_priv(dev);
	int nvfs[MLX4_MAX_PORTS + 1] = {0, 0, 0};
	int pci_dev_data, err, total_vfs;

	pci_dev_data = priv->pci_dev_data;
	total_vfs = dev->num_vfs;
	memcpy(nvfs, dev->nvfs, sizeof(dev->nvfs));

	mlx4_unload_one(pdev, dev);
	err = mlx4_load_one(pdev, pci_dev_data, total_vfs, nvfs, priv, 1);
	if (err) {
		mlx4_err(dev, "%s: ERROR: mlx4_load_one failed, pci_name=%s, err=%d\n",
			 __func__, pci_name(pdev), err);
		return err;
	}

	err = restore_current_port_types(dev, dev->curr_port_type, dev->curr_port_poss_type);
	if (err)
		mlx4_err(dev, "mlx4_restart_one: could not restore original port types (%d)\n",
			 err);
	return err;
}

static DEFINE_PCI_DEVICE_TABLE(mlx4_pci_table) = {
	/* MT25408 "Hermon" SDR */
	{ PCI_VDEVICE(MELLANOX, 0x6340), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" DDR */
	{ PCI_VDEVICE(MELLANOX, 0x634a), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" QDR */
	{ PCI_VDEVICE(MELLANOX, 0x6354), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" DDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6732), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" QDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x673c), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" EN 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6368), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25408 "Hermon" EN 10GigE PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6750), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25458 ConnectX EN 10GBASE-T 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6372), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25458 ConnectX EN 10GBASE-T+Gen2 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x675a), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT26468 ConnectX EN 10GigE PCIe gen2*/
	{ PCI_VDEVICE(MELLANOX, 0x6764), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT26438 ConnectX EN 40GigE PCIe gen2 5GT/s */
	{ PCI_VDEVICE(MELLANOX, 0x6746), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT26478 ConnectX2 40GigE PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x676e), MLX4_PCI_DEV_FORCE_SENSE_PORT },
	/* MT25400 Family [ConnectX-2 Virtual Function] */
	{ PCI_VDEVICE(MELLANOX, 0x1002), MLX4_PCI_DEV_IS_VF },
	/* MT27500 Family [ConnectX-3] */
	{ PCI_VDEVICE(MELLANOX, 0x1003), 0 },
	/* MT27500 Family [ConnectX-3 Virtual Function] */
	{ PCI_VDEVICE(MELLANOX, 0x1004), MLX4_PCI_DEV_IS_VF },
	{ PCI_VDEVICE(MELLANOX, 0x1005), 0 }, /* MT27510 Family */
	{ PCI_VDEVICE(MELLANOX, 0x1006), 0 }, /* MT27511 Family */
	{ PCI_VDEVICE(MELLANOX, 0x1007), 0 }, /* MT27520 Family */
	{ PCI_VDEVICE(MELLANOX, 0x1008), 0 }, /* MT27521 Family */
	{ PCI_VDEVICE(MELLANOX, 0x1009), 0 }, /* MT27530 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100a), 0 }, /* MT27531 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100b), 0 }, /* MT27540 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100c), 0 }, /* MT27541 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100d), 0 }, /* MT27550 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100e), 0 }, /* MT27551 Family */
	{ PCI_VDEVICE(MELLANOX, 0x100f), 0 }, /* MT27560 Family */
	{ PCI_VDEVICE(MELLANOX, 0x1010), 0 }, /* MT27561 Family */
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mlx4_pci_table);

static pci_ers_result_t mlx4_pci_err_detected(struct pci_dev *pdev,
					      pci_channel_state_t state)
{
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);

	mlx4_err(dev, "mlx4_pci_err_detected was called\n");
	mlx4_enter_error_state(dev);

	mutex_lock(&dev->interface_state_mutex);
	if (dev->interface_state & MLX4_INTERFACE_STATE_UP) {
		mlx4_err(dev, "mlx4_pci_err_detected: calling unload\n");
		mlx4_unload_one(pdev, dev);
		mlx4_err(dev, "mlx4_pci_err_detected: after unload\n");
	} else {
		mlx4_err(dev, "mlx4_pci_err_detected: NOP, state is not up\n");
	}
	mutex_unlock(&dev->interface_state_mutex);
	pci_disable_device(pdev);
	return state == pci_channel_io_perm_failure ?
		PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t mlx4_pci_slot_reset(struct pci_dev *pdev)
{
	int nvfs[MLX4_MAX_PORTS + 1] = {0, 0, 0};
	int total_vfs;
	int ret = 0;
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);
	struct mlx4_priv *priv = mlx4_priv(dev);

	mlx4_err(dev, "mlx4_pci_slot_reset was called\n");
	/* below stuff is persistent from the initial probe call */
	total_vfs = dev->num_vfs;
	memcpy(nvfs, dev->nvfs, sizeof(dev->nvfs));

	ret = pci_enable_device(pdev);
	if (ret) {
		mlx4_err(dev, "mlx4_pci_slot_reset: Can not re-enable device after reset\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}
	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	mutex_lock(&dev->interface_state_mutex);
	if (!(dev->interface_state & MLX4_INTERFACE_STATE_UP)) {
		ret = mlx4_load_one(pdev, 0, total_vfs, nvfs, priv, 1);
		mlx4_err(dev, "mlx4_pci_slot_reset: ret from mlx4_load_one is %d\n", ret);
		if (!ret) {
			ret = restore_current_port_types(dev, dev->curr_port_type, dev->curr_port_poss_type);
			if (ret)
				mlx4_err(dev, "mlx4_pci_slot_reset: could not restore original port types (%d)\n", ret);
		}
	} else {
		mlx4_err(dev, "mlx4_pci_slot_reset: interface is up, NOP\n");
	}
	mutex_unlock(&dev->interface_state_mutex);
	return ret ? PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_RECOVERED;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
static const struct pci_error_handlers mlx4_err_handler = {
#else
static struct pci_error_handlers mlx4_err_handler = {
#endif
	.error_detected = mlx4_pci_err_detected,
	.slot_reset     = mlx4_pci_slot_reset,
};

static void mlx4_shutdown(struct pci_dev *pdev)
{
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);

	mlx4_err(dev, "mlx4_shutdown was called\n");
	mutex_lock(&dev->interface_state_mutex);
	if (dev->interface_state & MLX4_INTERFACE_STATE_UP)
		mlx4_unload_one(pdev, dev);
	mutex_unlock(&dev->interface_state_mutex);

	return;
}

static int suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);

	mlx4_err(dev, "suspend was called\n");
	mutex_lock(&dev->interface_state_mutex);
	if (dev->interface_state & MLX4_INTERFACE_STATE_UP)
		mlx4_unload_one(pdev, dev);
	mutex_unlock(&dev->interface_state_mutex);

	return 0;
}

static int resume(struct pci_dev *pdev)
{
	int nvfs[MLX4_MAX_PORTS + 1] = {0, 0, 0};
	int total_vfs;
	struct mlx4_dev  *dev  = pci_get_drvdata(pdev);
	struct mlx4_priv *priv = mlx4_priv(dev);
	int ret = 0;

	mlx4_err(dev, "resume was called\n");
	/* below stuff is persistent from the initial probe call */
	total_vfs = dev->num_vfs;
	memcpy(nvfs, dev->nvfs, sizeof(dev->nvfs));

	mutex_lock(&dev->interface_state_mutex);
	if (!(dev->interface_state & MLX4_INTERFACE_STATE_UP)) {
		ret = mlx4_load_one(pdev, 0, total_vfs, nvfs, priv, 1);
		mlx4_err(dev, "resume: ret from mlx4_load_one is %d\n", ret);
		if (!ret) {
			ret = restore_current_port_types(dev, dev->curr_port_type, dev->curr_port_poss_type);
			if (ret)
				mlx4_err(dev, "resume: could not restore original port types (%d)\n", ret);
		}
	} else {
		mlx4_err(dev, "resume: interface is up, NOP\n");
	}
	mutex_unlock(&dev->interface_state_mutex);
	return ret;

}

static struct pci_driver mlx4_driver = {
	.name		= DRV_NAME,
	.id_table	= mlx4_pci_table,
	.probe		= mlx4_probe,
	.remove		= __devexit_p(mlx4_remove_one),
	.shutdown	= mlx4_shutdown,
	.suspend	= suspend,
	.resume		= resume,
	.err_handler    = &mlx4_err_handler,
};

static int __init mlx4_verify_params(void)
{
	int status;

	status = update_defaults(&port_type_array);
	if (status == INVALID_STR) {
		if (mlx4_fill_dbdf2val_tbl(&port_type_array.dbdf2val))
			return -1;
	} else if (status == INVALID_DATA) {
		return -1;
	}

	status = update_defaults(&num_vfs);
	if (status == INVALID_STR) {
		if (mlx4_fill_dbdf2val_tbl(&num_vfs.dbdf2val))
			return -1;
	} else if (status == INVALID_DATA) {
		return -1;
	}

	status = update_defaults(&probe_vf);
	if (status == INVALID_STR) {
		if (mlx4_fill_dbdf2val_tbl(&probe_vf.dbdf2val))
			return -1;
	} else if (status == INVALID_DATA) {
		return -1;
	}

	status = update_defaults(&roce_mode);
	if (status == INVALID_STR) {
		if (mlx4_fill_dbdf2val_tbl(&roce_mode.dbdf2val))
			return -1;
	} else if (status == INVALID_DATA) {
		return -1;
	}

	if (msi_x < 0) {
		pr_warn("mlx4_core: bad msi_x: %d\n", msi_x);
		return -1;
	}

	if ((log_num_mac < 0) || (log_num_mac > 7)) {
		pr_warning("mlx4_core: bad num_mac: %d\n", log_num_mac);
		return -1;
	}

	if (log_num_vlan != 0)
		pr_warning("mlx4_core: log_num_vlan - obsolete module param, using %d\n",
			   MLX4_LOG_NUM_VLANS);

	if (mlx4_set_4k_mtu != -1)
		pr_warning("mlx4_core: set_4k_mtu - obsolete module param\n");

	if ((log_mtts_per_seg < 0) || (log_mtts_per_seg > 7)) {
		pr_warning("mlx4_core: bad log_mtts_per_seg: %d\n", log_mtts_per_seg);
		return -1;
	}

	if (mlx4_log_num_mgm_entry_size < (int)(-MLX4_DMFS_PARAM_VALUES) ||
	    (mlx4_log_num_mgm_entry_size > 0 &&
	     (mlx4_log_num_mgm_entry_size < MLX4_MIN_MGM_LOG_ENTRY_SIZE ||
	     mlx4_log_num_mgm_entry_size > MLX4_MAX_MGM_LOG_ENTRY_SIZE))) {
		pr_warning("mlx4_core: mlx4_log_num_mgm_entry_size (%d) not "
			   "in legal range -%d..0 or %d..%d)\n",
			   mlx4_log_num_mgm_entry_size,
			   MLX4_DMFS_PARAM_VALUES,
			   MLX4_MIN_MGM_LOG_ENTRY_SIZE,
			   MLX4_MAX_MGM_LOG_ENTRY_SIZE);
		return -1;
	}

	if (mlx4_log_num_mgm_entry_size < 0 &&
	    (!((-mlx4_log_num_mgm_entry_size) & MLX4_DMFS_ETH_ONLY)) &&
	    ((-mlx4_log_num_mgm_entry_size) & MLX4_DMFS_A0_STEERING)) {
		pr_warn("mlx4_core: Can't support IPoIB flow steering along "
			"with optimized steering\n");
		return -1;
	}

	if (mod_param_profile.num_qp < 18 || mod_param_profile.num_qp > 23) {
		pr_warning("mlx4_core: bad log_num_qp: %d\n",
			   mod_param_profile.num_qp);
		return -1;
	}

	if (mod_param_profile.num_srq < 10) {
		pr_warning("mlx4_core: too low log_num_srq: %d\n",
			   mod_param_profile.num_srq);
		return -1;
	}

	if (mod_param_profile.num_cq < 10) {
		pr_warning("mlx4_core: too low log_num_cq: %d\n",
			   mod_param_profile.num_cq);
		return -1;
	}

	if (mod_param_profile.num_mpt < 10) {
		pr_warning("mlx4_core: too low log_num_mpt: %d\n",
			   mod_param_profile.num_mpt);
		return -1;
	}

	if (mod_param_profile.num_mtt_segs &&
	    mod_param_profile.num_mtt_segs < 15) {
		pr_warning("mlx4_core: too low log_num_mtt: %d\n",
			   mod_param_profile.num_mtt_segs);
		return -1;
	}

	if (mod_param_profile.num_mtt_segs > MLX4_MAX_LOG_NUM_MTT) {
		pr_warning("mlx4_core: too high log_num_mtt: %d\n",
			   mod_param_profile.num_mtt_segs);
		return -1;
	}
	return 0;
}

static int __init mlx4_init(void)
{
	int ret;

	if (mlx4_verify_params())
		return -EINVAL;

	mlx4_wq = create_singlethread_workqueue("mlx4");
	if (!mlx4_wq) {
		ret = -ENOMEM;
		goto out;
	}

	if (enable_sys_tune)
		sys_tune_init();

	ret = pci_register_driver(&mlx4_driver);
	if (ret < 0)
		goto err;

	return 0;

err:
	if (enable_sys_tune)
		sys_tune_fini();

	destroy_workqueue(mlx4_wq);
out:
	return ret;
}

static void __exit mlx4_cleanup(void)
{
	if (enable_sys_tune)
		sys_tune_fini();

	pci_unregister_driver(&mlx4_driver);
	destroy_workqueue(mlx4_wq);
}

module_init(mlx4_init);
module_exit(mlx4_cleanup);
