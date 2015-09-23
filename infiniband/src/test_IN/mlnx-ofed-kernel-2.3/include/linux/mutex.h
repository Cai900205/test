#ifndef _COMPAT_MUTEX_
#define _COMPAT_MUTEX_

#include_next <linux/mutex.h>

#if defined(COMPAT_VMWARE)
#define __mutex_init(mutex, name, key) do { \
	name; \
	key; \
	mutex_init(mutex); \
} while (0)
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_MUTEX_ */
