#ifndef LINUX_3_10_COMPAT_H
#define LINUX_3_10_COMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))

#ifndef NETIF_F_GSO_UDP_TUNNEL
#define NETIF_F_GSO_UDP_TUNNEL (1 << 25)
#endif

#else /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)) */

#include <linux/random.h>
#include <linux/netdev_features.h>

#ifndef NETIF_F_HW_VLAN_RX
#define NETIF_F_HW_VLAN_RX NETIF_F_HW_VLAN_CTAG_RX
#endif

#ifndef NETIF_F_HW_VLAN_TX
#define NETIF_F_HW_VLAN_TX NETIF_F_HW_VLAN_CTAG_TX
#endif

#ifndef NETIF_F_HW_VLAN_FILTER
#define NETIF_F_HW_VLAN_FILTER NETIF_F_HW_VLAN_CTAG_FILTER
#endif

#define random32() prandom_u32()

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)) */

#endif /* LINUX_3_10_COMPAT_H */
