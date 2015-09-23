#ifndef _COMPAT_SCHED_
#define _COMPAT_SCHED_

#include_next <linux/sched.h>

#if defined(COMPAT_VMWARE)

#include <linux/printk.h>

static inline int restart_syscall(void)
{
	printk(KERN_WARNING "called untested restart_syscall on vmklinux\n");
        /* set_tsk_thread_flag(current, TIF_SIGPENDING); */
        return -ERESTARTNOINTR;
}

#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_SCHED_ */
