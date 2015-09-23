#ifndef _COMPAT_PM_
#define _COMPAT_PM_

#include_next <linux/pm.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30))

/* How to reorder dpm_list after device_move() */
enum dpm_order {
	DPM_ORDER_NONE,
	DPM_ORDER_DEV_AFTER_PARENT,
	DPM_ORDER_PARENT_BEFORE_DEV,
	DPM_ORDER_DEV_LAST,
};

#endif

#endif /* _COMPAT_PM_ */
