#ifndef LINUX_3_17_COMPAT_H
#define LINUX_3_17_COMPAT_H

#include <linux/version.h>
#include <linux/dcbnl.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0) && !defined(COMPAT_VMWARE))

#ifndef CONFIG_COMPAT_IS_QCN

enum dcbnl_cndd_states {
	DCB_CNDD_RESET = 0,
	DCB_CNDD_EDGE,
	DCB_CNDD_INTERIOR,
	DCB_CNDD_INTERIOR_READY,
};

struct ieee_qcn {
	__u8 rpg_enable[IEEE_8021QAZ_MAX_TCS];
	__u32 rppp_max_rps[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_time_reset[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_byte_reset[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_threshold[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_max_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_ai_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_hai_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_gd[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_min_dec_fac[IEEE_8021QAZ_MAX_TCS];
	__u32 rpg_min_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 cndd_state_machine[IEEE_8021QAZ_MAX_TCS];
};

struct ieee_qcn_stats {
	__u64 rppp_rp_centiseconds[IEEE_8021QAZ_MAX_TCS];
	__u32 rppp_created_rps[IEEE_8021QAZ_MAX_TCS];
	__u32 ignored_cnm[IEEE_8021QAZ_MAX_TCS];
	__u32 estimated_total_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 cnms_handled_successfully[IEEE_8021QAZ_MAX_TCS];
	__u32 min_total_limiters_rate[IEEE_8021QAZ_MAX_TCS];
	__u32 max_total_limiters_rate[IEEE_8021QAZ_MAX_TCS];
};

#endif


#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,76,0)) */

#endif /* LINUX_3_17_COMPAT_H */
