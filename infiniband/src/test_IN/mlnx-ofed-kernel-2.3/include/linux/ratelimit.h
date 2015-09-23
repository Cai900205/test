#ifndef _COMPAT_RATELIMIT_
#define _COMPAT_RATELIMIT_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/ratelimit.h>

#else

#include_next <linux/ratelimit.h>

#endif /* COMPAT_VMWARE */

#endif
