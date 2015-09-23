#ifndef LINUX_3_13_COMPAT_H
#define LINUX_3_13_COMPAT_H

#include <linux/version.h>
#include <linux/completion.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0))

#ifndef CONFIG_COMPAT_IS_REINIT_COMPLETION
#define CONFIG_COMPAT_IS_REINIT_COMPLETION

static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}

#endif

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)) */

#endif /* LINUX_3_13_COMPAT_H */
