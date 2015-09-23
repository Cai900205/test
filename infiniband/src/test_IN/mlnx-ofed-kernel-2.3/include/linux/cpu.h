#ifndef _COMPAT_CPU_H_
#define _COMPAT_CPU_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <linux/cpu.h>
#endif

#endif /* _COMPAT_CPU_H_ */
