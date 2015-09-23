#ifndef LINUX_26_30_COMPAT_H
#define LINUX_26_30_COMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30))

#include <linux/device.h>

#ifndef TP_PROTO
#define TP_PROTO(args...)	TPPROTO(args)
#endif
#ifndef TP_ARGS
#define TP_ARGS(args...)	TPARGS(args)
#endif

#define IRQ_WAKE_THREAD	(2)

#ifndef COMPAT_VMWARE
static inline void dev_set_uevent_suppress(struct device *dev, int val)
{
	dev->uevent_suppress = val;
}
#endif

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)) */

#endif /* LINUX_26_30_COMPAT_H */
