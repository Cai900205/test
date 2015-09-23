#ifndef _COMPAT_CRED_H_
#define _COMPAT_CRED_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <linux/cred.h>
#endif

#endif /* _COMPAT_CRED_H_ */
