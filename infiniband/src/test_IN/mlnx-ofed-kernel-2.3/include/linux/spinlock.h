#ifndef _COMPAT_SPINLOCK_
#define _COMPAT_SPINLOCK_

#include_next <linux/spinlock.h>

#if defined(COMPAT_VMWARE)

/*
#define raw_spin_lock_init(lock)                               \
	do { *(lock) = __RAW_SPIN_LOCK_UNLOCKED; } while (0)
*/
static inline void raw_spin_lock_init(raw_spinlock_t *lock)
{
	const raw_spinlock_t tmp = __RAW_SPIN_LOCK_UNLOCKED;
	*lock = tmp;
}

#endif /* COMPAT_VMWARE */

#endif
