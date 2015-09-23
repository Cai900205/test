#ifndef LINUX_3_1_COMPAT_H
#define LINUX_3_1_COMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0) && !defined(COMPAT_VMWARE))

#include <linux/security.h>
#include <linux/skbuff.h>
#include <net/ip.h>
#include <linux/idr.h>
#include <net/dst.h>

#ifndef CONFIG_COMPAT_DST_NEIGHBOUR
static inline struct neighbour *dst_get_neighbour(struct dst_entry *dst)
{
	return dst->neighbour;
}

static inline void dst_set_neighbour(struct dst_entry *dst, struct neighbour *neigh)
{
	dst->neighbour = neigh;
}

static inline struct neighbour *dst_get_neighbour_raw(struct dst_entry *dst)
{
	return rcu_dereference_raw(dst->neighbour);
}
#endif /* CONFIG_COMPAT_DST_NEIGHBOUR */

/* Backports 56f8a75c */
static inline bool ip_is_fragment(const struct iphdr *iph)
{
	return (iph->frag_off & htons(IP_MF | IP_OFFSET)) != 0;
}

/* mask __netdev_alloc_skb_ip_align as RHEL6.3 backports it */
#define __netdev_alloc_skb_ip_align(a, b, c) compat__netdev_alloc_skb_ip_align(a, b, c)

static inline struct sk_buff *__netdev_alloc_skb_ip_align(struct net_device *dev,
							  unsigned int length, gfp_t gfp)
{
	struct sk_buff *skb = __netdev_alloc_skb(dev, length + NET_IP_ALIGN, gfp);

	if (NET_IP_ALIGN && skb)
		skb_reserve(skb, NET_IP_ALIGN);
	return skb;
}

#if ! defined(CONFIG_COMPAT_MIN_DUMP_ALLOC_ARG) && ! defined(CONFIG_COMPAT_NETLINK_3_7)
#include <linux/netlink.h>
/* remove last arg */
#define netlink_dump_start(a, b, c, d, e, f) netlink_dump_start(a, b, c, d, e)
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

#define IFF_TX_SKB_SHARING	0x10000	/* The interface supports sharing
					 * skbs on transmit */

#define PCMCIA_DEVICE_MANF_CARD_PROD_ID3(manf, card, v3, vh3) { \
	.match_flags = PCMCIA_DEV_ID_MATCH_MANF_ID| \
			PCMCIA_DEV_ID_MATCH_CARD_ID| \
			PCMCIA_DEV_ID_MATCH_PROD_ID3, \
	.manf_id = (manf), \
	.card_id = (card), \
	.prod_id = { NULL, NULL, (v3), NULL }, \
	.prod_id_hash = { 0, 0, (vh3), 0 }, }

/*
 * This has been defined in include/linux/security.h for some time, but was
 * only given an EXPORT_SYMBOL for 3.1.  Add a compat_* definition to avoid
 * breaking the compile.
 */
#define security_sk_clone(a, b) compat_security_sk_clone(a, b)

static inline void security_sk_clone(const struct sock *sk, struct sock *newsk)
{
}

/*
 * In many versions, several architectures do not seem to include an
 * atomic64_t implementation, and do not include the software emulation from
 * asm-generic/atomic64_t.
 * Detect and handle this here.
 */
#include <asm/atomic.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)) && !defined(ATOMIC64_INIT) && !defined(CONFIG_X86) && !((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)) && defined(CONFIG_ARM) && !defined(CONFIG_GENERIC_ATOMIC64))
#include <asm-generic/atomic64.h>
#endif

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)) */

#endif /* LINUX_3_1_COMPAT_H */
