#ifndef _COMPAT_POLL_
#define _COMPAT_POLL_

#include_next <linux/poll.h>

#if defined(COMPAT_VMWARE)
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_POLL_ */
