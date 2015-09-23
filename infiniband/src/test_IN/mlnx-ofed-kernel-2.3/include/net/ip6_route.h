#ifndef _COMPAT_IP6_ROUTE_H_
#define _COMPAT_IP6_ROUTE_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <net/ip6_route.h>
#endif

#endif /* _COMPAT_IP6_ROUTE_H_ */
