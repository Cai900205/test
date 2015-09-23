#ifndef _LOCKDEP_
#define _LOCKDEP_

#include_next <linux/lockdep.h>

#if defined(COMPAT_VMWARE)
#define lockdep_assert_held(l)                  do { (void)(l); } while (0)
#define lockdep_set_novalidate_class(lock) do { } while (0)
#endif /* COMPAT_VMWARE */

#endif /* _LOCKDEP_ */
