#ifndef _COMPAT_COMPILER_GCC3_
#define _COMPAT_COMPILER_GCC3_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/compiler-gcc3.h>

#else

#include_next <linux/compiler-gcc3.h>

#endif /* COMPAT_VMWARE */

#endif
