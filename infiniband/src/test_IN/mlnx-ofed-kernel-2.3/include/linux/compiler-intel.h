#ifndef _COMPAT_COMPILER_INTEL_
#define _COMPAT_COMPILER_INTEL_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/compiler-intel.h>

#else

#include_next <linux/compiler-intel.h>

#endif /* COMPAT_VMWARE */

#endif
