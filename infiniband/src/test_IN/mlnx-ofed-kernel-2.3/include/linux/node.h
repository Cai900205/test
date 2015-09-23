#ifndef _COMPAT_NODE_H_
#define _COMPAT_NODE_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <linux/node.h>
#endif

#endif /* _COMPAT_NODE_H_ */
