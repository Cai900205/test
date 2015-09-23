#ifndef _COMPAT_STRING_
#define _COMPAT_STRING_

#include_next <linux/string.h>

#if defined(COMPAT_VMWARE)

extern bool sysfs_streq(const char *s1, const char *s2);
extern int strtobool(const char *s, bool *res);

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_STRING_ */
