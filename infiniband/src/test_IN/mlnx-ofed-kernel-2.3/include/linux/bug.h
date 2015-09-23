#ifndef _COMPAT_BUG_
#define _COMPAT_BUG_

#if defined(COMPAT_VMWARE)

#include <linux/kernel.h>

#else

#include_next <linux/bug.h>

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_BUG_ */
