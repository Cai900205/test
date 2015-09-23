#ifndef _SLAB_
#define _SLAB_

#include_next <linux/slab.h>

#if defined(COMPAT_VMWARE)
#define kmalloc_track_caller(size, flags) \
	kmalloc(size, flags)
#define kzalloc_node(_size, _flags, _node) kzalloc(_size, _flags)
#endif /* COMPAT_VMWARE */

#endif /* _SLAB_ */
