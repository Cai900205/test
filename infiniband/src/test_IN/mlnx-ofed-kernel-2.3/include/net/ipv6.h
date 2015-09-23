#ifndef _COMPAT_IPV6_H_
#define _COMPAT_IPV6_H_

#if defined(COMPAT_VMWARE)


static inline void __ipv6_addr_set_half(__be32 *addr,
					__be32 wh, __be32 wl)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
#if defined(__BIG_ENDIAN)
	if (__builtin_constant_p(wh) && __builtin_constant_p(wl)) {
		*(__force u64 *)addr = ((__force u64)(wh) << 32 | (__force u64)(wl));
		return;
	}
#elif defined(__LITTLE_ENDIAN)
	if (__builtin_constant_p(wl) && __builtin_constant_p(wh)) {
		*(__force u64 *)addr = ((__force u64)(wl) << 32 | (__force u64)(wh));
		return;
	}
#endif
#endif
	addr[0] = wh;
	addr[1] = wl;
}

static inline void ipv6_addr_set(struct in6_addr *addr, 
				     __be32 w1, __be32 w2,
				     __be32 w3, __be32 w4)
{
	__ipv6_addr_set_half(&addr->s6_addr32[0], w1, w2);
	__ipv6_addr_set_half(&addr->s6_addr32[2], w3, w4);
}

static inline bool ipv6_addr_v4mapped(const struct in6_addr *a)
{
	return (
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
		*(__be64 *)a |
#else
		(a->s6_addr32[0] | a->s6_addr32[1]) |
#endif
		(a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0UL;
}

static inline void ipv6_addr_set_v4mapped(const __be32 addr,
					  struct in6_addr *v4mapped)
{
	ipv6_addr_set(v4mapped,
			0, 0,
			htonl(0x0000FFFF),
			addr);
}


static inline void ipv6_eth_mc_map(const struct in6_addr *addr, char *buf)
{
	/*
	 *	+-------+-------+-------+-------+-------+-------+
	 *      |   33  |   33  | DST13 | DST14 | DST15 | DST16 |
	 *      +-------+-------+-------+-------+-------+-------+
	 */

	buf[0]= 0x33;
	buf[1]= 0x33;

	memcpy(buf + 2, &addr->s6_addr32[3], sizeof(__u32));
}


#else
#include_next <net/ipv6.h>
#endif

#endif /* _COMPAT_IPV6_H_ */
