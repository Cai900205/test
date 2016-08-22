/* Copyright (c) 2011 - 2012 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define SRIO_PORT_MAX_NUM	2	/* SRIO port max number */
#define SRIO_OB_WIN_NUM		9	/* SRIO outbound window number */
#define SRIO_IB_WIN_NUM		5	/* SRIO inbound window number */
#define SRIO_MAX_SEG_NUM	4
#define SRIO_MAX_SUBSEG_NUM	8
#define SRIO_ADDR_SHIFT		12
#define SRIO_ISR_AACR_AA	0x1	/* Accept All ID */
#define SRIO_RIWAR_MEM		0x80f55000
#define SRIO_CCSR_PT		0x1
#define SRIO_CCSR_PW0_1X	0x02000000
#define SRIO_CCSR_OPE_IPE_EN	0x00600000
#define SRIO_CCSR_DPE_OFFSET	2
#define SRIO_CCSR_PD_OFFSET	23
#define SRIO_CCSR_IPW_OFFSET	27
#define SRIO_CCSR_IPW_MASK	0x7
#define SRIO_DSLLCSR_TM_OFFSET	24
#define SRIO_PCR_OBDEN_OFFSET	2
#define SRIO_LIVE_TIME_SHIFT	8
#define SRIO_ROWAR_WR_MASK	(0xff << 12)
#define SRIO_ROWAR_EN_WIN	(0x1 << 31)
#define SRIO_ROWAR_NSGE_SHIFT	22
#define SRIO_ROWAR_NSEG_MASK	(0x3 << 22)
#define SRIO_ROWAR_NSSEG_MASK	(0x3 << 20)
#define SRIO_ROWAR_NSSEG_SHIFT	20
#define SRIO_ROWAR_RDTYP_SHIFT	16
#define SRIO_ROWAR_WRTYP_SHIFT	12
#define SRIO_ADIDCSR_ADID_SHIFT	16
#define SRIO_ADIDCSR_ADID_MASK	(0xff << 16)
#define SRIO_ADIDCSR_ADE_EN	(0x1 << 31)
#define SRIO_ROWTAR_TREXAD_SHIFT	22
#define SRIO_ROWTAR_TREXAD_MASK	(0x3ff << 20)
#define SRIO_ROWBAR_SIZE_MASK	0x3f
#define SRIO_ROWSR_WRTYP_SHIFT	16
#define SRIO_ROWSR_RDTYP_SHIFT	20
#define SRIO_ROWSR_TDID_MASK	0xff
#define SRIO_ROWSR_WR_MASK	(0xff << 16)
#define SRIO_REG_8BIT_MASK	0xff
#define SRIO_SLEICR_EIC_OFFSET	27
#define SRIO_ERTCSR_ERDTT_OFFSET	16
#define SRIO_ERTCSR_ERFTT_OFFSET	24
#define SRIO_CCSR_SPF_DPE_OFFSET	2

/* Architectural regsiters */
struct rio_arch {
	uint32_t	didcar;		/* 0xc_0000 */
	uint32_t	dicar;		/* 0xc_0004 */
	uint32_t	aidcar;		/* 0xc_0008 */
	uint32_t	aicar;		/* 0xc_000c */
	uint32_t	pefcar;		/* 0xc_0010 */
	uint32_t	res0;		/* 0xc_0014 */
	uint32_t	socar;		/* 0xc_0018 */
	uint32_t	docar;		/* 0xc_001c */
	uint32_t	res1[7];	/* 0xc_0020 - 0xc_003b */
	uint32_t	dsicar;		/* 0xc_003c */
	uint32_t	res2[2];	/* 0xc_0040 - 0xc_0047 */
	uint32_t	dsllcsr;	/* 0xc_0048 */
	uint32_t	pellccsr;	/* 0xc_004c */
	uint32_t	res3[3];	/* 0xc_0050 - 0xc_005b */
	uint32_t	lcsba1csr;	/* 0xc_005c */
	uint32_t	bdidcsr;	/* 0xc_0060 */
	uint32_t	res4;		/* 0xc_0064 */
	uint32_t	hbdidlcsr;	/* 0xc_0068 */
	uint32_t	ctcsr;		/* 0xc_006c */
};

/* Extended Features Space: 1x/4x LP-Serial Port registers */
struct rio_lp_serial_port {
	uint32_t	lmreqcsr;	/* 0xc_0140/0xc_0160 */
	uint32_t	lmrespcsr;	/* 0xc_0144/0xc_0164 */
	uint32_t	lascsr;		/* 0xc_0148/0xc_0168 */
	uint32_t	res0[3];	/* 0xc_014c - 0xc_0157 */
					/* 0xc_016c - 0xc_0177 */
	uint32_t	escsr;		/* 0xc_0158/0xc_0178 */
	uint32_t	ccsr;		/* 0xc_015c/0xc_017c */
};

/* Extended Features Space: 1x/4x LP-Serial registers*/
struct rio_lp_serial {
	uint32_t	pmbh0;		/* 0xc_0100 */
	uint32_t	res0[7];	/* 0xc_0104 - 0xc_011f */
	uint32_t	pltoccsr;	/* 0xc_0120 */
	uint32_t	prtoccsr;	/* 0xc_0124 */
	uint32_t	res1[5];	/* 0xc_0128 - 0xc_013b */
	uint32_t	pgccsr;		/* 0xc_013c */
	struct rio_lp_serial_port	port[SRIO_PORT_MAX_NUM];
};

/* Logical error reporting registers */
struct rio_logical_err {
	uint32_t	erbh;		/* 0xc_0600 */
	uint32_t	res0;		/* 0xc_0604 */
	uint32_t	ltledcsr;	/* 0xc_0608 */
	uint32_t	ltleecsr;	/* 0xc_060c */
	uint32_t	res1;		/* 0xc_0610 */
	uint32_t	ltlaccsr;	/* 0xc_0614 */
	uint32_t	ltldidccsr;	/* 0xc_0618 */
	uint32_t	ltlcccsr;	/* 0xc_061c */
};

/* Physical error reporting port registers */
struct rio_phys_err_port {
	uint32_t	edcsr;		/* 0xc_0640/0xc_0680 */
	uint32_t	erecsr;		/* 0xc_0644/0xc_0684 */
	uint32_t	ecacsr;		/* 0xc_0648/0xc_0688 */
	uint32_t	pcseccsr0;	/* 0xc_064c/0xc_068c */
	uint32_t	peccsr[3];	/* 0xc_0650/0xc_0698 */
	uint32_t	res0[3];	/* 0xc_065c - 0xc_0667 */
					/* 0xc_069c - 0xc_06a7 */
	uint32_t	ercsr;		/* 0xc_0668/0xc_06a8 */
	uint32_t	ertcsr;		/* 0xc_066c/0xc_06ac */
	uint32_t	res1[4];	/* 0xc_0670 - 0x_067f */
					/* 0xc_06b0 - 0x_06bf */
};

/* Physical error reporting registers */
struct rio_phys_err {
	struct rio_phys_err_port	port[SRIO_PORT_MAX_NUM];
};

/* Implementation Space: General Port-Common */
struct rio_impl_common {
	uint32_t	res0;		/* 0xd_0000 */
	uint32_t	llcr;		/* 0xd_0004 */
	uint32_t	res1[2];	/* 0xd_0008 - 0xd_000f */
	uint32_t	epwisr;		/* 0xd_0010 */
	uint32_t	res2[3];	/* 0xd_0014 - 0xd_001f */
	uint32_t	lretcr;		/* 0xd_0020 */
	uint32_t	res3[23];	/* 0xd_0024 - 0xd_007f */
	uint32_t	pretcr;		/* 0xd_0080 */
	uint32_t	res4[31];
};

/* Implementation Space: Port Specific */
struct rio_impl_port_spec {
	uint32_t	adidcsr;	/* 0xd_0100/0xd_0180 */
	uint32_t	res0[7];	/* 0xd_0104 - 0xd_011f */
					/* 0xd_0184 - 0xd_019f */
	uint32_t	accr;		/* 0xd_0120/0xd_01a0 */
	uint32_t	lopttlcr;	/* 0xd_0124/0xd_01a4 */
	uint32_t	res1[2];	/* 0xd_0128 - 0xd_012f */
					/* 0xd_01a8 - 0xd_01af */
	uint32_t	iecsr;		/* 0xd_0130/0xd_01b0 */
	uint32_t	res2[3];	/* 0xd_0134 - 0xd_013f */
					/* 0xd_01b4 - 0xd_01bf */
	uint32_t	pcr;		/* 0xd_0140/0xd_01c0 */
	uint32_t	res3[5];	/* 0xd_0144 - 0xd_0157 */
					/* 0xd_01c4 - 0xd_01d7 */
	uint32_t	slcsr;		/* 0xd_0158/0xd_01d8 */
	uint32_t	res4;		/* 0xd_015c/0xd_01dc */
	uint32_t	sleicr;		/* 0xd_0160/0xd_01e0 */
	uint32_t	a0txcr;		/* 0xd_0164/0xd_01e4 */
	uint32_t	a1txcr;		/* 0xd_0168/0xd_01e8 */
	uint32_t	a2txcr;		/* 0xd_016c/0xd_01ec */
	uint32_t	mreqtxbacr[3];	/* 0xd_0170 - 0xd_017b/0xd_01f0 -  0xd_01fb*/
	uint32_t	mrspfctxbacr;	/* 0xd_017c/0xd_01fc */
};

/* Implementation Space: register */
struct rio_implement {
	struct rio_impl_common		com;
	struct rio_impl_port_spec	port[SRIO_PORT_MAX_NUM];
};

/* Revision Control Register */
struct rio_rev_ctrl {
	uint32_t	ipbrr[2];	/* 0xd_0bf8 - 0xd_0bff*/
};

struct rio_atmu_row {
	uint32_t	rowtar;
	uint32_t	rowtear;
	uint32_t	rowbar;
	uint32_t	res0;
	uint32_t	rowar;
	uint32_t	rowsr[3];
};

struct rio_atmu_riw {
	uint32_t	riwtar;
	uint32_t	res0;
	uint32_t	riwbar;
	uint32_t	res1;
	uint32_t	riwar;
	uint32_t	res2[3];
};

/* ATMU window registers */
struct rio_atmu_win {
	struct rio_atmu_row	outbw[SRIO_OB_WIN_NUM];
	uint32_t	res0[16];
	struct rio_atmu_riw	inbw[SRIO_IB_WIN_NUM];
};

struct rio_atmu {
	struct rio_atmu_win	port[SRIO_PORT_MAX_NUM];
};

struct rio_regs {
	struct rio_arch		arch;
	uint32_t	res0[36];	/* 0xc_0070 - 0xc_00ff */
	struct rio_lp_serial	lp_serial;
	uint32_t	res2[288];	/* 0xc_0180 - 0xc_05ff */
	struct rio_logical_err	logical_err;
	uint32_t	res3[8];	/* 0xc_0620 - 0xc_063f */
	struct rio_phys_err	phys_err;
	uint32_t res4[15952];		/* 0xc_06c0 - 0xc_ffff */
	struct rio_implement	impl;
	uint32_t res5[638];		/* 0xd_0200 - 0xd_0bf7 */
	struct rio_rev_ctrl rev;
	struct rio_atmu		atmu;
};

struct addr_info {
	uint64_t start;
	uint64_t size;
};

struct srio_port_info {
	struct addr_info range;
};

struct srio_port {
	uint8_t enable;
	uint8_t port_id;
	struct addr_info win_range;
	void *mem_win;
	int port_fd;
	void *priv;
};

struct srio_dev {
	struct rio_regs *rio_regs;
	uint64_t regs_size;
	int reg_fd;
	struct srio_port *port;
	uint32_t port_num;
};
