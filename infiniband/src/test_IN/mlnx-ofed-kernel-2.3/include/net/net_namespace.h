#ifndef _COMPAT_NET_NAMESPACE_H_
#define _COMPAT_NET_NAMESPACE_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <net/net_namespace.h>
#endif

#endif /* _COMPAT_NET_NAMESPACE_H_ */
