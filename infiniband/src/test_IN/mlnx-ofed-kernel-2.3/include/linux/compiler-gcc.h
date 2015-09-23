#ifndef _COMPAT_COMPILER_GCC_
#define _COMPAT_COMPILER_GCC_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/compiler-gcc.h>

#else

#include_next <linux/compiler-gcc.h>

#endif /* COMPAT_VMWARE */

#endif
