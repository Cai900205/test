#ifndef _COMPAT_CPUMASK_
#define _COMPAT_CPUMASK_

#include_next <linux/cpumask.h>

#if defined(COMPAT_VMWARE)

#include <linux/errno.h>
typedef struct cpumask *cpumask_var_t;

static inline int irq_set_affinity_hint(unsigned int irq,
					const struct cpumask *m)
{
	return -EINVAL;
}

static inline bool cpumask_empty(const struct cpumask *srcp)
{
	return true;
}
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_CPUMASK_ */
