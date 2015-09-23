#ifndef _COMPAT_RBTREE_
#define _COMPAT_RBTREE_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/rbtree.h>

#else

#include_next <linux/rbtree.h>

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_RBTREE_ */
