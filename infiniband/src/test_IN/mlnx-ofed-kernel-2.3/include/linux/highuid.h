#ifndef _COMPAT_HIGHUID_
#define _COMPAT_HIGHUID_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/highuid.h>

#else

#include_next <linux/highuid.h>

#endif /* COMPAT_VMWARE */

#endif
