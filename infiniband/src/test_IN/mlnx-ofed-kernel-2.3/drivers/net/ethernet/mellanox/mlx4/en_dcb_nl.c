/*
 * Copyright (c) 2011 Mellanox Technologies. All rights reserved.
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
 *
 */

#include <linux/dcbnl.h>
#include <linux/math64.h>

#include "mlx4_en.h"

/* Callbacks for DCB netlink in the kernel */
#define BIT_PFC		0x02

/* Responses for the DCB_C_SET_ALL command */
#define DCB_HW_CHG_RST	0  /* DCB configuration changed with reset */
#define DCB_NO_HW_CHG	1  /* DCB configuration did not change */
#define DCB_HW_CHG	2  /* DCB configuration changed, no reset */

/* Definitions for QCN
 */

#define MLX4_CMD_CONGESTION_CTRL_OPCODE 0x68

struct congestion_control_mb_prio_802_1_qau_params {
	__be32 modify_enable_high;
	__be32 modify_enable_low;
	__be32 reserved1;
	__be32 extended_enable;
	__be32 rppp_max_rps;
	__be32 rpg_time_reset;
	__be32 rpg_byte_reset;
	__be32 rpg_threshold;
	__be32 rpg_max_rate;
	__be32 rpg_ai_rate;
	__be32 rpg_hai_rate;
	__be32 rpg_gd;
	__be32 rpg_min_dec_fac;
	__be32 rpg_min_rate;
	__be32 max_time_rise;
	__be32 max_byte_rise;
	__be32 max_qdelta;
	__be32 min_qoffset;
	__be32 gd_coefficient;
	__be32 reserved2[5];
	__be32 cp_sample_base;
	__be32 reserved3[39];
} __packed;

struct congestion_control_mb_prio_802_1_qau_statistics {
	__be64 rppp_rp_centiseconds;
	__be32 reserved1;
	__be32 ignored_cnm;
	__be32 rppp_created_rps;
	__be32 estimated_total_rate;
	__be32 max_active_rate_limiter_index;
	__be32 dropped_cnms_busy_fw;
	__be32 reserved2;
	__be32 cnms_handled_successfully;
	__be32 min_total_limiters_rate;
	__be32 max_total_limiters_rate;
	__be32 reserved3[4];
} __packed;

enum mlx4_en_congestion_control_algorithm {
	MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT = 0,
};

enum mlx4_en_congestion_control_opmod {
	MLX4_CONGESTION_CONTROL_GET_PARAMS,
	MLX4_CONGESTION_CONTROL_GET_STATISTICS,
	MLX4_CONGESTION_CONTROL_SET_PARAMS = 4,
};

static int mlx4_en_copy_dcb_cfg(struct mlx4_en_priv *priv, int tc_max)
{
	struct mlx4_en_dcb_config *scfg = &priv->temp_dcb_cfg;
	struct mlx4_en_dcb_config *dcfg = &priv->dcb_cfg;
	int i, j;
	int changes = 0;

	for (i = DCB_PFC_UP_ATTR_0; i < DCB_PFC_UP_ATTR_MAX; i++) {
		j = i - DCB_PFC_UP_ATTR_0;
		if (dcfg->tc_config[j].dcb_pfc != scfg->tc_config[j].dcb_pfc) {
			dcfg->tc_config[j].dcb_pfc = scfg->tc_config[j].dcb_pfc;
			changes |= BIT_PFC;
		}
	}

	if (dcfg->pfc_mode_enable != scfg->pfc_mode_enable) {
		dcfg->pfc_mode_enable = scfg->pfc_mode_enable;
		changes |= BIT_PFC;
	}

	return changes;
}


static u8 mlx4_en_dcbnl_getcap(struct net_device *dev, int capid, u8 *cap)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	switch (capid) {
	case DCB_CAP_ATTR_PFC:
		*cap = true;
		break;
	case DCB_CAP_ATTR_DCBX:
		*cap = priv->dcbx_cap;
		break;
	case DCB_CAP_ATTR_PFC_TCS:
		*cap = (priv->dcbx_cap & DCB_CAP_DCBX_VER_CEE) ? 0x80 : false;
		break;
	default:
		*cap = false;
		break;
	}

	return 0;
}

static u8 mlx4_en_dcbnl_getpfcstate(struct net_device *netdev)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);

	return priv->dcb_cfg.pfc_mode_enable;
}

static void mlx4_en_dcbnl_setpfcstate(struct net_device *netdev, u8 state)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);

	priv->temp_dcb_cfg.pfc_mode_enable = state;
}

static void mlx4_en_dcbnl_get_pfc_cfg(struct net_device *netdev, int priority,
				      u8 *setting)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);

	*setting = priv->dcb_cfg.tc_config[priority].dcb_pfc;
}

static void mlx4_en_dcbnl_set_pfc_cfg(struct net_device *netdev, int priority,
				      u8 setting)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);

	priv->temp_dcb_cfg.tc_config[priority].dcb_pfc = setting;
	if (priv->temp_dcb_cfg.tc_config[priority].dcb_pfc !=
	    priv->dcb_cfg.tc_config[priority].dcb_pfc)
		priv->temp_dcb_cfg.pfc_mode_enable = true;
}

#ifdef CONFIG_COMPAT_SETNUMTCS_INT
static int mlx4_en_dcbnl_getnumtcs(struct net_device *netdev, int tcid, u8 *num)
#else
static u8 mlx4_en_dcbnl_getnumtcs(struct net_device *netdev, int tcid, u8 *num)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);

	if (!(priv->flags & MLX4_EN_FLAG_DCB_ENABLED))
		return -EINVAL;
	if (!(tcid == DCB_NUMTCS_ATTR_PFC))
		*num = 0;
	else
		*num = MLX4_EN_NUM_UP;

	return 0;
}
#ifdef CONFIG_COMPAT_SETNUMTCS_INT
static int mlx4_en_dcbnl_setnumtcs(struct net_device *netdev, int tcid, u8 num)
#else
static u8 mlx4_en_dcbnl_setnumtcs(struct net_device *netdev, int tcid, u8 num)
#endif
{
	return (num == MLX4_EN_NUM_UP) ? 0 : -EINVAL;
}

static u8 mlx4_en_dcbnl_set_all(struct net_device *netdev)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	struct mlx4_en_dcb_config *dcb_cfg = &priv->dcb_cfg;
	int ret = DCB_NO_HW_CHG;
	int tc;

	/* Fail command if not in CEE mode */
	if (!(priv->dcbx_cap & DCB_CAP_DCBX_VER_CEE))
		return ret;
	priv->dcb_set_bitmap |= mlx4_en_copy_dcb_cfg(priv,
						     MLX4_EN_NUM_UP);
	if (!priv->dcb_set_bitmap)
		return ret;
	if (priv->dcb_set_bitmap & BIT_PFC) {
		if (dcb_cfg->pfc_mode_enable) {
			priv->prof->rx_pause = 0;
			priv->prof->tx_pause = 0;
			for (tc = 0; tc < MLX4_EN_NUM_UP; tc++) {
				switch (dcb_cfg->tc_config[tc].dcb_pfc) {
				case pfc_disabled:
					priv->prof->tx_ppp |= 0 << tc;
					priv->prof->rx_ppp |= 0 << tc;
					break;
				case pfc_enabled_full:
					priv->prof->tx_ppp |= 1 << tc;
					priv->prof->rx_ppp |= 1 << tc;
					break;
				case pfc_enabled_tx:
					priv->prof->tx_ppp |= 1 << tc;
					priv->prof->rx_ppp |= 0 << tc;
					break;
				case pfc_enabled_rx:
					priv->prof->tx_ppp |= 0 << tc;
					priv->prof->rx_ppp |= 1 << tc;
					break;
				default:
					break;
				}
			}
			en_info(priv, "Set pfc on\n");
		} else {
			priv->prof->rx_pause = 1;
			priv->prof->tx_pause = 1;
			en_info(priv, "Set pfc off\n");
		}

		ret = mlx4_en_restorepfc(netdev);
		if (ret) {
			en_err(priv, "Failed setting pause params\n");
			return DCB_NO_HW_CHG;
		}
		ret = DCB_HW_CHG;
	}
	priv->dcb_set_bitmap = 0x00;
	return ret;
}

static u8 mlx4_en_dcbnl_get_state(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	return !!(priv->flags & MLX4_EN_FLAG_DCB_ENABLED);
}

static u8 mlx4_en_dcbnl_set_state(struct net_device *dev, u8 state)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int err = 0;

	/* Fail command if not in CEE mode */
	if (!(priv->dcbx_cap & DCB_CAP_DCBX_VER_CEE))
		return 1;

	/* verify there is something to do, if not then exit */
	if (!state == !(priv->flags & MLX4_EN_FLAG_DCB_ENABLED))
		return 0;

	if (state) {
		priv->flags |= MLX4_EN_FLAG_DCB_ENABLED;
		err = mlx4_en_setup_tc(dev, MLX4_EN_NUM_UP);
	} else {
		priv->flags &= ~MLX4_EN_FLAG_DCB_ENABLED;
		err = mlx4_en_setup_tc(dev, 0);
	}

	return err;
}

/**
 * mlx4_en_dcbnl_getapp - retrieve the DCBX application user priority
 * @netdev : the corresponding netdev
 * @idtype : identifies the id as ether type or TCP/UDP port number
 * @id: id is either ether type or TCP/UDP port number
 *
 * Returns : on success, returns a non-zero 802.1p user priority bitmap
 * otherwise returns 0 as the invalid user priority bitmap to indicate an
 * error.
 */

static u8 mlx4_en_dcbnl_getapp(struct net_device *netdev, u8 idtype, u16 id)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	struct dcb_app app = {
				.selector = idtype,
				.protocol = id,
			     };
	if (!(priv->dcbx_cap & DCB_CAP_DCBX_VER_CEE))
		return 0;
	return dcb_getapp(netdev, &app);
}

/**
 * mlx4_en_dcbnl_setapp - add CEE dcb application data to app list.
 */

static u8 mlx4_en_dcbnl_setapp(struct net_device *netdev, u8 idtype,
				u16 id, u8 up)
{
	struct dcb_app app;
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	int ret = -EINVAL;

	if (!(priv->dcbx_cap & DCB_CAP_DCBX_VER_CEE))
		return ret;

	app.selector = idtype;
	app.protocol = id;
	app.priority = up;
	ret = dcb_setapp(netdev, &app);

	return ret;
}

int mlx4_en_dcbnl_ieee_getets(struct net_device *dev,
			      struct ieee_ets *ets)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct ieee_ets *my_ets = &priv->ets;

	/* No IEEE PFC settings available */
	if (!my_ets)
		return -EINVAL;

	ets->ets_cap = IEEE_8021QAZ_MAX_TCS;
	ets->cbs = my_ets->cbs;
	memcpy(ets->tc_tx_bw, my_ets->tc_tx_bw, sizeof(ets->tc_tx_bw));
	memcpy(ets->tc_tsa, my_ets->tc_tsa, sizeof(ets->tc_tsa));
	memcpy(ets->prio_tc, my_ets->prio_tc, sizeof(ets->prio_tc));

	return 0;
}

static int mlx4_en_ets_validate(struct mlx4_en_priv *priv, struct ieee_ets *ets)
{
	int i;
	int total_ets_bw = 0;
	int has_ets_tc = 0;

	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		if (ets->prio_tc[i] >= MLX4_EN_NUM_UP) {
			en_err(priv, "Bad priority in UP <=> TC mapping. TC: %d, UP: %d\n",
					i, ets->prio_tc[i]);
			return -EINVAL;
		}

		switch (ets->tc_tsa[i]) {
		case IEEE_8021QAZ_TSA_STRICT:
			break;
		case IEEE_8021QAZ_TSA_ETS:
			has_ets_tc = 1;
			total_ets_bw += ets->tc_tx_bw[i];
			break;
		default:
			en_err(priv, "TC[%d]: Not supported TSA: %d\n",
					i, ets->tc_tsa[i]);
			return -ENOTSUPP;
		}
	}

	if (has_ets_tc && total_ets_bw != MLX4_EN_BW_MAX) {
		en_err(priv, "Bad ETS BW sum: %d. Should be exactly 100%%\n",
				total_ets_bw);
		return -EINVAL;
	}

	return 0;
}

int mlx4_disable_32_14_4_e_read(struct mlx4_dev *dev, u8 *config, int port)
{
	struct congestion_control_mb_prio_802_1_qau_params *hw_qcn;
	struct mlx4_cmd_mailbox *mailbox_out = NULL;
	u64 mailbox_in_dma = 0;
	u32 inmod = 0;
	int err = 0;

	if (dev->caps.ecn_qcn_ver == 0)
		return -EOPNOTSUPP;

	mailbox_out = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox_out))
		return -ENOMEM;

	hw_qcn =
	(struct congestion_control_mb_prio_802_1_qau_params *)
	mailbox_out->buf;

	inmod = port | 1 << 8 |
		(MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT << 16);

	err = mlx4_cmd_box(dev, mailbox_in_dma,
			   mailbox_out->dma,
			   inmod, MLX4_CONGESTION_CONTROL_GET_PARAMS,
			   MLX4_CMD_CONGESTION_CTRL_OPCODE,
			   MLX4_CMD_TIME_CLASS_C,
			   MLX4_CMD_NATIVE);
	if (!err)
		*config = be32_to_cpu(hw_qcn->extended_enable) >> 22;

	mlx4_free_cmd_mailbox(dev, mailbox_out);

	return err;
}

int mlx4_disable_32_14_4_e_write(struct mlx4_dev *dev, u8 config, int port)
{
	struct congestion_control_mb_prio_802_1_qau_params *hw_qcn;
	struct mlx4_cmd_mailbox *mailbox_in = NULL;
	u64 mailbox_in_dma = 0;
	u32 inmod = 0;
	int err;

	if (dev->caps.ecn_qcn_ver == 0)
		return -EOPNOTSUPP;

	mailbox_in = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox_in))
		return -ENOMEM;

	mailbox_in_dma = mailbox_in->dma;
	hw_qcn =
	(struct congestion_control_mb_prio_802_1_qau_params *)mailbox_in->buf;

	inmod = port | 0xff << 8 |
		(MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT << 16);

	/* Before updating QCN parameter,
	 *need to set it's modify enable bit to 1
	 */

	hw_qcn->modify_enable_high = cpu_to_be32(1 << 22);

	hw_qcn->extended_enable = cpu_to_be32(config << 22);

	err = mlx4_cmd(dev, mailbox_in_dma, inmod,
		       MLX4_CONGESTION_CONTROL_SET_PARAMS,
		       MLX4_CMD_CONGESTION_CTRL_OPCODE,
		       MLX4_CMD_TIME_CLASS_C,
		       MLX4_CMD_NATIVE);

	mlx4_free_cmd_mailbox(dev, mailbox_in);
	return err;
}

static int mlx4_en_config_port_scheduler(struct mlx4_en_priv *priv,
		struct ieee_ets *ets, u16 *ratelimit)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int num_strict = 0;
	int i;
	__u8 tc_tx_bw[IEEE_8021QAZ_MAX_TCS] = { 0 };
	__u8 pg[IEEE_8021QAZ_MAX_TCS] = { 0 };

	ets = ets ?: &priv->ets;
	ratelimit = ratelimit ?: priv->maxrate;

	/* higher TC means higher priority => lower pg */
	for (i = IEEE_8021QAZ_MAX_TCS - 1; i >= 0; i--) {
		switch (ets->tc_tsa[i]) {
		case IEEE_8021QAZ_TSA_STRICT:
			pg[i] = num_strict++;
			tc_tx_bw[i] = MLX4_EN_BW_MAX;
			break;
		case IEEE_8021QAZ_TSA_ETS:
			pg[i] = MLX4_EN_TC_ETS;
			tc_tx_bw[i] = ets->tc_tx_bw[i] ?: MLX4_EN_BW_MIN;
			break;
		}
	}

	return mlx4_SET_PORT_SCHEDULER(mdev->dev, priv->port, tc_tx_bw, pg,
			ratelimit);
}

int mlx4_en_dcbnl_ieee_setets(struct net_device *dev, struct ieee_ets *ets)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	err = mlx4_en_ets_validate(priv, ets);
	if (err)
		return err;

	err = mlx4_SET_PORT_PRIO2TC(mdev->dev, priv->port, ets->prio_tc);
	if (err)
		return err;

	err = mlx4_en_config_port_scheduler(priv, ets, NULL);
	if (err)
		return err;

	memcpy(&priv->ets, ets, sizeof(priv->ets));

	return 0;
}

int mlx4_en_dcbnl_ieee_getpfc(struct net_device *dev,
			      struct ieee_pfc *pfc)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	pfc->pfc_cap = IEEE_8021QAZ_MAX_TCS;
	pfc->pfc_en = priv->prof->tx_ppp;

	return 0;
}

int mlx4_en_restorepfc(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	if (!priv->prof->tx_pause &&
	    !priv->prof->tx_ppp   &&
	    !priv->prof->rx_pause &&
	    !priv->prof->rx_ppp)
		return 0;

	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_skb_size + ETH_FCS_LEN,
				    priv->prof->tx_pause,
				    priv->prof->tx_ppp,
				    priv->prof->rx_pause,
				    priv->prof->rx_ppp);
	if (err)
		en_err(priv, "Failed setting pause params\n");

	return err;
}

static int mlx4_en_dcbnl_ieee_setpfc(struct net_device *dev,
				     struct ieee_pfc *pfc)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	en_dbg(DRV, priv, "cap: 0x%x en: 0x%x mbc: 0x%x delay: %d\n",
	       pfc->pfc_cap,
	       pfc->pfc_en,
	       pfc->mbc,
	       pfc->delay);

	priv->prof->tx_pause = !pfc->pfc_en;
	priv->prof->rx_pause = !pfc->pfc_en;
	priv->prof->tx_ppp = pfc->pfc_en;
	priv->prof->rx_ppp = pfc->pfc_en;

	return mlx4_en_restorepfc(dev);
}

static u8 mlx4_en_dcbnl_getdcbx(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	return priv->dcbx_cap;
}

static u8 mlx4_en_dcbnl_setdcbx(struct net_device *dev, u8 mode)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct ieee_ets ets = {0};
	struct ieee_pfc pfc = {0};
	int err = 0;

	/* support only DCB_CAP_DCBX_VER_CEE or DCB_CAP_DCBX_VER_IEEE modes */
	if ((mode & DCB_CAP_DCBX_LLD_MANAGED) ||
	    ((mode & DCB_CAP_DCBX_VER_IEEE) && (mode & DCB_CAP_DCBX_VER_CEE)) ||
	    !(mode & DCB_CAP_DCBX_HOST))
		return 1;

	if (mode == priv->dcbx_cap)
		return 0;

	priv->dcbx_cap = mode;

	/* ETS and PFC defaults */
	ets.ets_cap = 8;
	pfc.pfc_cap = 8;

	if (mode & DCB_CAP_DCBX_VER_IEEE) {
		err = mlx4_en_dcbnl_ieee_setets(dev, &ets);
		if (err)
			return 1;
		err = mlx4_en_dcbnl_ieee_setpfc(dev, &pfc);
		if (err)
			return 1;
	} else if (mode & DCB_CAP_DCBX_VER_CEE) {
		priv->dcb_set_bitmap |= BIT_PFC;
		err = mlx4_en_dcbnl_set_all(dev);
		if (err)
			return 1;
	} else {
		/* Drop into single TC mode strict priority as this
		 * indicates CEE and IEEE versions are disabled
		 */
		err = mlx4_en_dcbnl_ieee_setets(dev, &ets);
		if (err)
			return 1;
		err = mlx4_en_dcbnl_ieee_setpfc(dev, &pfc);
		if (err)
			return 1;
		err = mlx4_en_setup_tc(dev, 0);
	}

	return err ? 1 : 0;
}

#define MLX4_RATELIMIT_UNITS_IN_KB 100000 /* rate-limit HW unit in Kbps */
int mlx4_en_dcbnl_ieee_getmaxrate(struct net_device *dev,
				  struct ieee_maxrate *maxrate)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int i;

	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++)
		maxrate->tc_maxrate[i] =
			priv->maxrate[i] * MLX4_RATELIMIT_UNITS_IN_KB;

	return 0;
}

int mlx4_en_dcbnl_ieee_setmaxrate(struct net_device *dev,
				  struct ieee_maxrate *maxrate)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	u16 tmp[IEEE_8021QAZ_MAX_TCS];
	int i, err;

	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		/* Convert from Kbps into HW units, rounding result up.
		 * Setting to 0, means unlimited BW.
		 */
		tmp[i] = div_u64(maxrate->tc_maxrate[i] +
				 MLX4_RATELIMIT_UNITS_IN_KB - 1,
				 MLX4_RATELIMIT_UNITS_IN_KB);
	}

	err = mlx4_en_config_port_scheduler(priv, NULL, tmp);
	if (err)
		return err;

	memcpy(priv->maxrate, tmp, sizeof(priv->maxrate));

	return 0;
}

#ifdef CONFIG_COMPAT_IS_QCN
static int mlx4_en_dcbnl_ieee_getqcn(struct net_device *dev,
					struct ieee_qcn *qcn)
#else
int mlx4_en_dcbnl_ieee_getqcn(struct net_device *dev,
				struct ieee_qcn *qcn)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct congestion_control_mb_prio_802_1_qau_params *hw_qcn;
	struct mlx4_cmd_mailbox *mailbox_out = NULL;
	u64 mailbox_in_dma = 0;
	u32 inmod = 0;
	int i, err;

	if (priv->mdev->dev->caps.ecn_qcn_ver == 0)
		return -EOPNOTSUPP;

	mailbox_out = mlx4_alloc_cmd_mailbox(priv->mdev->dev);
	if (IS_ERR(mailbox_out))
		return -ENOMEM;
	hw_qcn =
	(struct congestion_control_mb_prio_802_1_qau_params *)
	mailbox_out->buf;

	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		inmod = priv->port | ((1<<i) << 8) |
			 (MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT << 16);
		err = mlx4_cmd_box(priv->mdev->dev, mailbox_in_dma,
				   mailbox_out->dma,
				   inmod, MLX4_CONGESTION_CONTROL_GET_PARAMS,
				   MLX4_CMD_CONGESTION_CTRL_OPCODE,
				   MLX4_CMD_TIME_CLASS_C,
				   MLX4_CMD_NATIVE);
		if (!err) {
			qcn->rpg_enable[i] =
				be32_to_cpu(hw_qcn->extended_enable) >> 31;
			qcn->rppp_max_rps[i] =
				be32_to_cpu(hw_qcn->rppp_max_rps);
			qcn->rpg_time_reset[i] =
				be32_to_cpu(hw_qcn->rpg_time_reset);
			qcn->rpg_byte_reset[i] =
				be32_to_cpu(hw_qcn->rpg_byte_reset);
			qcn->rpg_threshold[i] =
				be32_to_cpu(hw_qcn->rpg_threshold);
			qcn->rpg_max_rate[i] =
				be32_to_cpu(hw_qcn->rpg_max_rate);
			qcn->rpg_ai_rate[i] =
				be32_to_cpu(hw_qcn->rpg_ai_rate);
			qcn->rpg_hai_rate[i] =
				be32_to_cpu(hw_qcn->rpg_hai_rate);
			qcn->rpg_gd[i] =
				be32_to_cpu(hw_qcn->rpg_gd);
			qcn->rpg_min_dec_fac[i] =
				be32_to_cpu(hw_qcn->rpg_min_dec_fac);
			qcn->rpg_min_rate[i] =
				be32_to_cpu(hw_qcn->rpg_min_rate);
			qcn->cndd_state_machine[i] =
				priv->cndd_state[i];
		} else {
			mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_out);
			return err;
		}
	}
	mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_out);
	return 0;
}

#ifdef CONFIG_COMPAT_IS_QCN
static int mlx4_en_dcbnl_ieee_setqcn(struct net_device *dev,
					struct ieee_qcn *qcn)
#else
int mlx4_en_dcbnl_ieee_setqcn(struct net_device *dev,
				struct ieee_qcn *qcn)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct congestion_control_mb_prio_802_1_qau_params *hw_qcn;
	struct mlx4_cmd_mailbox *mailbox_in = NULL;
	u64 mailbox_in_dma = 0;
	u32 inmod = 0;
	int i, err;

	if (priv->mdev->dev->caps.ecn_qcn_ver == 0)
		return -EOPNOTSUPP;

	mailbox_in = mlx4_alloc_cmd_mailbox(priv->mdev->dev);
	if (IS_ERR(mailbox_in))
		return -ENOMEM;

	mailbox_in_dma = mailbox_in->dma;
	hw_qcn =
	(struct congestion_control_mb_prio_802_1_qau_params *)mailbox_in->buf;
	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		inmod = priv->port | ((1<<i) << 8) |
			 (MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT << 16);

		/* Before updating QCN parameter,
		 * need to set it's modify enable bit to 1
		 */

		hw_qcn->modify_enable_high = cpu_to_be32(1 << 31);
		hw_qcn->modify_enable_high |= cpu_to_be32(1 << 30);
		hw_qcn->modify_enable_low = cpu_to_be32(1 << 31);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 30);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 29);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 28);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 27);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 26);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 25);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 24);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 23);
		hw_qcn->modify_enable_low |= cpu_to_be32(1 << 22);

		hw_qcn->extended_enable = cpu_to_be32(qcn->rpg_enable[i] << 31);
		hw_qcn->rppp_max_rps = cpu_to_be32(qcn->rppp_max_rps[i]);
		hw_qcn->rpg_time_reset = cpu_to_be32(qcn->rpg_time_reset[i]);
		hw_qcn->rpg_byte_reset = cpu_to_be32(qcn->rpg_byte_reset[i]);
		hw_qcn->rpg_threshold = cpu_to_be32(qcn->rpg_threshold[i]);
		hw_qcn->rpg_max_rate = cpu_to_be32(qcn->rpg_max_rate[i]);
		hw_qcn->rpg_ai_rate = cpu_to_be32(qcn->rpg_ai_rate[i]);
		hw_qcn->rpg_hai_rate = cpu_to_be32(qcn->rpg_hai_rate[i]);
		hw_qcn->rpg_gd = cpu_to_be32(qcn->rpg_gd[i]);
		hw_qcn->rpg_min_dec_fac = cpu_to_be32(qcn->rpg_min_dec_fac[i]);
		hw_qcn->rpg_min_rate = cpu_to_be32(qcn->rpg_min_rate[i]);
		priv->cndd_state[i] = qcn->cndd_state_machine[i];
		if (qcn->cndd_state_machine[i] == DCB_CNDD_INTERIOR_READY)
			hw_qcn->extended_enable |= cpu_to_be32(1 << 30);

		err = mlx4_cmd(priv->mdev->dev, mailbox_in_dma, inmod,
			       MLX4_CONGESTION_CONTROL_SET_PARAMS,
			       MLX4_CMD_CONGESTION_CTRL_OPCODE,
			       MLX4_CMD_TIME_CLASS_C,
			       MLX4_CMD_NATIVE);
		if (err) {
			mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_in);
			return err;
		}
	}
	mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_in);
	return 0;
}

#ifdef CONFIG_COMPAT_IS_QCN
static int mlx4_en_dcbnl_ieee_getqcnstats(struct net_device *dev,
					  struct ieee_qcn_stats *qcn_stats)
#else
int mlx4_en_dcbnl_ieee_getqcnstats(struct net_device *dev,
				struct ieee_qcn_stats *qcn_stats)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct congestion_control_mb_prio_802_1_qau_statistics *hw_qcn_stats;
	struct mlx4_cmd_mailbox *mailbox_out = NULL;
	u64 mailbox_in_dma = 0;
	u32 inmod = 0;
	int i, err;

	if (priv->mdev->dev->caps.ecn_qcn_ver == 0)
		return -EOPNOTSUPP;

	mailbox_out = mlx4_alloc_cmd_mailbox(priv->mdev->dev);
	if (IS_ERR(mailbox_out))
		return -ENOMEM;

	hw_qcn_stats =
	(struct congestion_control_mb_prio_802_1_qau_statistics *)
	mailbox_out->buf;

	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		inmod = priv->port | ((1<<i) << 8) |
			 (MLX4_CTRL_ALGO_802_1_QAU_REACTION_POINT << 16);
		err = mlx4_cmd_box(priv->mdev->dev, mailbox_in_dma,
				   mailbox_out->dma, inmod,
				   MLX4_CONGESTION_CONTROL_GET_STATISTICS,
				   MLX4_CMD_CONGESTION_CTRL_OPCODE,
				   MLX4_CMD_TIME_CLASS_C,
				   MLX4_CMD_NATIVE);
		if (!err) {
			qcn_stats->rppp_rp_centiseconds[i] =
				be64_to_cpu(
				hw_qcn_stats->rppp_rp_centiseconds);
			qcn_stats->rppp_created_rps[i] =
				be32_to_cpu(
				hw_qcn_stats->rppp_created_rps);
			qcn_stats->ignored_cnm[i] =
				be32_to_cpu(
				hw_qcn_stats->ignored_cnm);
			qcn_stats->estimated_total_rate[i] =
				be32_to_cpu(
				hw_qcn_stats->estimated_total_rate);
			qcn_stats->cnms_handled_successfully[i] =
				be32_to_cpu(
				hw_qcn_stats->cnms_handled_successfully);
			qcn_stats->min_total_limiters_rate[i] =
				be32_to_cpu(
				hw_qcn_stats->min_total_limiters_rate);
			qcn_stats->max_total_limiters_rate[i] =
				be32_to_cpu(
				hw_qcn_stats->max_total_limiters_rate);
		} else {
			mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_out);
			return err;
		}
	}
	mlx4_free_cmd_mailbox(priv->mdev->dev, mailbox_out);
	return 0;
}

const struct dcbnl_rtnl_ops mlx4_en_dcbnl_ops = {
	.getstate	= mlx4_en_dcbnl_get_state,
	.ieee_getets	= mlx4_en_dcbnl_ieee_getets,
	.ieee_setets	= mlx4_en_dcbnl_ieee_setets,
#ifdef CONFIG_COMPAT_IS_MAXRATE
	.ieee_getmaxrate = mlx4_en_dcbnl_ieee_getmaxrate,
	.ieee_setmaxrate = mlx4_en_dcbnl_ieee_setmaxrate,
#endif
	.ieee_getpfc	= mlx4_en_dcbnl_ieee_getpfc,
	.ieee_setpfc	= mlx4_en_dcbnl_ieee_setpfc,
	.getcap		= mlx4_en_dcbnl_getcap,
	.getdcbx	= mlx4_en_dcbnl_getdcbx,
	.setdcbx	= mlx4_en_dcbnl_setdcbx,
	.setall		= mlx4_en_dcbnl_set_all,
	.setstate	= mlx4_en_dcbnl_set_state,
	.getpfcstate	= mlx4_en_dcbnl_getpfcstate,
	.setpfcstate	= mlx4_en_dcbnl_setpfcstate,
	.getpfccfg	= mlx4_en_dcbnl_get_pfc_cfg,
	.setpfccfg	= mlx4_en_dcbnl_set_pfc_cfg,
	.getnumtcs	= mlx4_en_dcbnl_getnumtcs,
	.setnumtcs	= mlx4_en_dcbnl_setnumtcs,
	.getapp		= mlx4_en_dcbnl_getapp,
	.setapp		= mlx4_en_dcbnl_setapp,
#ifdef CONFIG_COMPAT_IS_QCN
	.ieee_getqcn    = mlx4_en_dcbnl_ieee_getqcn,
	.ieee_setqcn    = mlx4_en_dcbnl_ieee_setqcn,
	.ieee_getqcnstats = mlx4_en_dcbnl_ieee_getqcnstats,
#endif
};

const struct dcbnl_rtnl_ops mlx4_en_dcbnl_pfc_ops = {
	.getstate	= mlx4_en_dcbnl_get_state,
	.ieee_getpfc	= mlx4_en_dcbnl_ieee_getpfc,
	.ieee_setpfc	= mlx4_en_dcbnl_ieee_setpfc,
	.getcap		= mlx4_en_dcbnl_getcap,
	.getdcbx	= mlx4_en_dcbnl_getdcbx,
	.setdcbx	= mlx4_en_dcbnl_setdcbx,
	.setall		= mlx4_en_dcbnl_set_all,
	.setstate	= mlx4_en_dcbnl_set_state,
	.getpfcstate	= mlx4_en_dcbnl_getpfcstate,
	.setpfcstate	= mlx4_en_dcbnl_setpfcstate,
	.getpfccfg	= mlx4_en_dcbnl_get_pfc_cfg,
	.setpfccfg	= mlx4_en_dcbnl_set_pfc_cfg,
	.getnumtcs	= mlx4_en_dcbnl_getnumtcs,
	.setnumtcs	= mlx4_en_dcbnl_setnumtcs,
	.getapp		= mlx4_en_dcbnl_getapp,
	.setapp		= mlx4_en_dcbnl_setapp,
#ifdef CONFIG_COMPAT_IS_QCN
	.ieee_getqcn	= mlx4_en_dcbnl_ieee_getqcn,
	.ieee_setqcn	= mlx4_en_dcbnl_ieee_setqcn,
	.ieee_getqcnstats = mlx4_en_dcbnl_ieee_getqcnstats,
#endif
};
