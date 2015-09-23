#ifndef _COMPAT_INET_H_
#define _COMPAT_INET_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <linux/inet.h>
#endif

#endif /* _COMPAT_INET_H_ */
