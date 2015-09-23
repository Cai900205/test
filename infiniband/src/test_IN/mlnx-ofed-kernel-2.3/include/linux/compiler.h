#ifndef _COMPAT_COMPILER_
#define _COMPAT_COMPILER_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/compiler.h>

#define __attribute_pure__ __pure
#undef __cond_lock
#ifdef __CHECKER__
# define __cond_lock(x) ((x) ? ({ __acquire(x); 1; }) : 0)
#else
# define __cond_lock(x) (x)
#endif

#else

#include_next <linux/compiler.h>

#endif /* COMPAT_VMWARE */

#endif
