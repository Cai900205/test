#ifndef _COMPAT_NETEVENT_H_
#define _COMPAT_NETEVENT_H_

#if defined(COMPAT_VMWARE)

#else
#include_next <net/netevent.h>
#endif

#endif /* _COMPAT_NETEVENT_H_ */
