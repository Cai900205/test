#ifndef _COMPAT_ARP_H_
#define _COMPAT_ARP_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <net/arp.h>
#endif

#endif /* _COMPAT_ARP_H_ */
