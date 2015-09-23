#ifndef _COMPAT_TYPES_
#define _COMPAT_TYPES_

#include_next <linux/types.h>

#if defined(COMPAT_VMWARE)
typedef u64 phys_addr_t;
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_TYPES_ */
