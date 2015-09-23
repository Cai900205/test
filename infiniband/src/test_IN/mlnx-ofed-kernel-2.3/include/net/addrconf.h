#ifndef _COMPAT_ADDRCONF_H_
#define _COMPAT_ADDRCONF_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <net/addrconf.h>
#endif

#endif /* _COMPAT_ADDRCONF_H_ */
