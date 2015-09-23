#ifndef LINUX_3_12_COMPAT_H
#define LINUX_3_12_COMPAT_H

#include <linux/version.h>
#include <linux/netdevice.h>

#ifndef IFF_EIPOIB_VIF
#define IFF_EIPOIB_VIF  0x800       /* IPoIB VIF intf(eg ib0.x, ib1.x etc.), using IFF_DONT_BRIDGE */
#endif

/* Added IFF_SLAVE_NEEDARP for SLES11SP1 Errata kernels where this was replaced
 * by IFF_MASTER_NEEDARP
 */
#ifndef IFF_SLAVE_NEEDARP
#define IFF_SLAVE_NEEDARP 0x40          /* need ARPs for validation     */
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0))

#define debugfs_create_atomic_t LINUX_BACKPORT(debugfs_create_atomic_t)
struct dentry *debugfs_create_atomic_t(const char *name, umode_t mode,
				       struct dentry *parent, atomic_t *value);

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)) */
#endif /* LINUX_3_12_COMPAT_H */
