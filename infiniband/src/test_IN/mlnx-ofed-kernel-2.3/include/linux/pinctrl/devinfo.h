#ifndef _COMPAT_PINCTRL_DEVINFO_
#define _COMPAT_PINCTRL_DEVINFO_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/pinctrl/devinfo.h>

#else

#include_next <linux/pinctrl/devinfo.h>

#endif /* COMPAT_VMWARE */

#endif
