#ifndef _COMPAT_RBTREE_AUGMENTED_
#define _COMPAT_RBTREE_AUGMENTED_

#if defined(COMPAT_VMWARE)

#include <linux-3.13/rbtree_augmented.h>

#else

#include_next <linux/rbtree_augmented.h>

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_RBTREE_AUGMENTED_ */
