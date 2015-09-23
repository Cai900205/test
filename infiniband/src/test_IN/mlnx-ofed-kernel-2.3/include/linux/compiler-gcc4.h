#ifndef _COMPAT_COMPILER_GCC4_
#define _COMPAT_COMPILER_GCC4_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/compiler-gcc4.h>

#else

#include_next <linux/compiler-gcc4.h>

#endif /* COMPAT_VMWARE */

#endif
