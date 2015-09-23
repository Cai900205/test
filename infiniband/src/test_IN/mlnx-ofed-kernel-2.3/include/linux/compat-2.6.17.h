#ifndef LINUX_26_17_COMPAT_H
#define LINUX_26_17_COMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sch_generic.h>
#include <linux/rcupdate.h>

#ifndef __packed
#define __packed                     __attribute__((packed))
#endif

#ifndef __percpu
#define __percpu
#endif

#ifndef find_last_bit
/**
 * find_last_bit - find the last set bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit number of the first set bit, or size.
 */
extern unsigned long find_last_bit(const unsigned long *addr,
				   unsigned long size);
#endif

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __ARG_PLACEHOLDER_1 0,
#define config_enabled(cfg) _config_enabled(cfg)
#define _config_enabled(value) __config_enabled(__ARG_PLACEHOLDER_##value)
#define __config_enabled(arg1_or_junk) ___config_enabled(arg1_or_junk 1, 0)
#define ___config_enabled(__ignored, val, ...) val

/*
 * IS_ENABLED(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y' or 'm',
 * 0 otherwise.
 *
 */
#define IS_ENABLED(option) \
        (config_enabled(option) || config_enabled(option##_MODULE))

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

static inline void put_pid(struct pid *pid)
{
	return;
}

static inline struct pid *get_task_pid(struct task_struct *task, int type)
{
	/* Not supported */
	return NULL;
}

/* dummy struct */
struct srcu_struct {
};

static inline int init_srcu_struct(struct srcu_struct *srcu)
{
	/* SRCU is not supported on SLES10,and its logic do nothing, added dummy stubs to prevent
	  * the need for multiple ifdefs via backporting.
	*/
	return 0;
}

static inline void cleanup_srcu_struct(struct srcu_struct *sp)
{
	return;
}

static inline int srcu_read_lock(struct srcu_struct *srcu)
{
	return 0;
}

static inline void srcu_read_unlock(struct srcu_struct *srcu, int key)
{
	return;
}

static inline void synchronize_srcu(struct srcu_struct *srcu)
{
	return;
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) || defined(COMPAT_VMWARE))
#ifndef __WARN
#define __WARN(foo) dump_stack()
#endif

#ifndef __WARN_printf
#define __WARN_printf(arg...)   do { printk(arg); __WARN(); } while (0)
#endif

#ifndef WARN
#define WARN(condition, format...) ({					\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN_printf(format);					\
	unlikely(__ret_warn_on);					\
})
#endif

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifndef sysfs_attr_init
#define sysfs_attr_init(attr) do {} while (0)
#endif

#ifndef MAX_IDR_MASK
#define MAX_IDR_SHIFT (sizeof(int)*8 - 1)
#define MAX_IDR_BIT (1U << MAX_IDR_SHIFT)
#define MAX_IDR_MASK (MAX_IDR_BIT - 1)
#endif
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) || defined(COMPAT_VMWARE) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))

static inline void netif_tx_wake_all_queues(struct net_device *dev)
{
	netif_wake_queue(dev);
}
static inline void netif_tx_start_all_queues(struct net_device *dev)
{
	netif_start_queue(dev);
}
static inline void netif_tx_stop_all_queues(struct net_device *dev)
{
	netif_stop_queue(dev);
}

/* Are all TX queues of the device empty?  */
static inline bool qdisc_all_tx_empty(const struct net_device *dev)
{
	return skb_queue_empty(&dev->qdisc->q);
}

#ifndef max3
#define max3(x, y, z) ({			\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	typeof(z) _max3 = (z);			\
	(void) (&_max1 == &_max2);		\
	(void) (&_max1 == &_max3);		\
	_max1 > _max2 ? (_max1 > _max3 ? _max1 : _max3) : \
		(_max2 > _max3 ? _max2 : _max3); })
#endif

#define NETIF_F_LOOPBACK       (1 << 31) /* Enable loopback */

#ifndef NETIF_F_RXCSUM
#define NETIF_F_RXCSUM		(1 << 29)
#endif

#ifndef rtnl_dereference
#define rtnl_dereference(p)                                     \
        rcu_dereference_protected(p, lockdep_rtnl_is_held())
#endif

#ifndef rcu_dereference_protected
#define rcu_dereference_protected(p, c) \
		rcu_dereference((p))
#endif

#ifndef rcu_dereference_bh
#define rcu_dereference_bh(p) \
		rcu_dereference((p))
#endif

static inline u16 skb_get_queue_mapping(struct sk_buff *skb)
{
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
	return skb->queue_mapping;
#else
	return 0;
#endif
}

static inline long __must_check IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef u64 phys_addr_t;
#else
typedef u32 phys_addr_t;
#endif

#ifndef PCI_VDEVICE
#define PCI_VDEVICE(vendor, device)             \
	PCI_VENDOR_ID_##vendor, (device),       \
	PCI_ANY_ID, PCI_ANY_ID, 0, 0
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) || defined(COMPAT_VMWARE))

#ifndef DEFINE_PCI_DEVICE_TABLE
#define DEFINE_PCI_DEVICE_TABLE(_table) \
	const struct pci_device_id _table[]
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17) || defined(COMPAT_VMWARE) */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))

#ifndef FIELD_SIZEOF
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#endif

/*
 * Backports for affinity hint feature - deprecated on SLES10.2
 */
typedef struct cpumask *cpumask_var_t;
#ifndef CONFIG_COMPAT_HAS_IRQ_AFFINITY_HINT
static inline int irq_set_affinity_hint(unsigned int irq,
					const struct cpumask *m)
{
	return -ENOSYS;
}

static inline bool cpumask_empty(const struct cpumask *srcp)
{
	return true;
}
#endif

/*
 * Backport work for QoS dependencies (kernel/pm_qos_params.c)
 * pm-qos stuff written by mark gross mgross@linux.intel.com.
 *
 * ipw2100 now makes use of:
 *
 * pm_qos_add_requirement(),
 * pm_qos_update_requirement() and
 * pm_qos_remove_requirement() from it
 *
 * mac80211 uses the network latency to determine if to enable or not
 * dynamic PS. mac80211 also and registers a notifier for when
 * the latency changes. Since older kernels do no thave pm-qos stuff
 * we just implement it completley here and register it upon cfg80211
 * init. I haven't tested ipw2100 on 2.6.24 though.
 *
 * This pm-qos implementation is copied verbatim from the kernel
 * written by mark gross mgross@linux.intel.com. You don't have
 * to do anythinig to use pm-qos except use the same exported
 * routines as used in newer kernels. The backport_pm_qos_power_init()
 * defned below is used by the compat module to initialize pm-qos.
 */
int backport_pm_qos_power_init(void);
int backport_pm_qos_power_deinit(void);

int backport_system_workqueue_create(void);
void backport_system_workqueue_destroy(void);

#define FMODE_PATH	((__force fmode_t)0x4000)

#define alloc_workqueue(name, flags, max_active) __create_workqueue(name, max_active)

extern void bitmap_set(unsigned long *map, int i, int len);
extern void bitmap_clear(unsigned long *map, int start, int nr);
extern unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int nr,
					 unsigned long align_mask);

#if !defined(CONFIG_COMPAT_IFLA_VF_LINK_STATE_MAX)
enum {
	IFLA_VF_LINK_STATE_AUTO,	/* link state of the uplink */
	IFLA_VF_LINK_STATE_ENABLE,	/* link always up */
	IFLA_VF_LINK_STATE_DISABLE,	/* link always down */
	__IFLA_VF_LINK_STATE_MAX,
};
#endif

#ifndef CONFIG_COMPAT_IS_REINIT_COMPLETION
#define CONFIG_COMPAT_IS_REINIT_COMPLETION
static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}

#endif

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_struct within the struct.
 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_struct within the struct.
 */
#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member) 			\
	for (; &pos->member != (head);					\
	     pos = list_next_entry(pos, member))

#define min3(x, y, z) ({			\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	typeof(z) _min3 = (z);			\
	(void) (&_min1 == &_min2);		\
	(void) (&_min1 == &_min3);		\
	_min1 < _min2 ? (_min1 < _min3 ? _min1 : _min3) : \
		(_min2 < _min3 ? _min2 : _min3); })

#define max3(x, y, z) ({			\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	typeof(z) _max3 = (z);			\
	(void) (&_max1 == &_max2);		\
	(void) (&_max1 == &_max3);		\
	_max1 > _max2 ? (_max1 > _max3 ? _max1 : _max3) : \
		(_max2 > _max3 ? _max2 : _max3); })

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)) */

#endif /* LINUX_26_17_COMPAT_H */
