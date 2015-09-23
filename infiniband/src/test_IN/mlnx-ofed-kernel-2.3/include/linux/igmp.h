#ifndef _COMPAT_IGMP_H_
#define _COMPAT_IGMP_H_

#if defined(COMPAT_VMWARE)

/*
 * IGMP protocol structures
 */

/*
 * Header in on cable format
 */

struct igmphdr {
	__u8 type;
	__u8 code; /* For newer IGMP */
	__sum16 csum;
	__be32 group;
};

struct igmpv3_grec {
	__u8 grec_type;
	__u8 grec_auxwords;
	__be16 grec_nsrcs;
	__be32 grec_mca;
	__be32 grec_src[0];
};

struct igmpv3_report {
	__u8 type;
	__u8 resv1;
	__be16 csum;
	__be16 resv2;
	__be16 ngrec;
	struct igmpv3_grec grec[0];
};

struct igmpv3_query {
	__u8 type;
	__u8 code;
	__be16 csum;
	__be32 group;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 qrv:3,
	suppress:1,
	resv:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8 resv:4,
	suppress:1,
	qrv:3;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	__u8 qqic;
	__be16 nsrcs;
	__be32 srcs[0];
};

#define IGMP_HOST_MEMBERSHIP_QUERY     0x11  /* From RFC1112 */
#define IGMP_HOST_MEMBERSHIP_REPORT    0x12  /* Ditto */
#define IGMPV2_HOST_MEMBERSHIP_REPORT  0x16  /* V2 version of 0x11 */
#define IGMPV3_HOST_MEMBERSHIP_REPORT  0x22  /* V3 version of 0x11 */

#ifdef __KERNEL__
#include <linux/skbuff.h>

static inline struct igmphdr *igmp_hdr(const struct sk_buff *skb)
{
	return (struct igmphdr *) skb_transport_header(skb);
}

static inline struct igmpv3_report *
igmpv3_report_hdr(const struct sk_buff *skb)
{
	return (struct igmpv3_report *) skb_transport_header(skb);
}

static inline struct igmpv3_query *
igmpv3_query_hdr(const struct sk_buff *skb)
{
	return (struct igmpv3_query *) skb_transport_header(skb);
}

#endif /* __KERNEL__ */

#else
#include_next <linux/igmp.h>
#endif

#endif /* _COMPAT_IGMP_H_ */
