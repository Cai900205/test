#ifndef _COMPAT_OF_DEVICE_
#define _COMPAT_OF_DEVICE_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/of_device.h>

#else

#include_next <linux/of_device.h>

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_OF_DEVICE_ */
