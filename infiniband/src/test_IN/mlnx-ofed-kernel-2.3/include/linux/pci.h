#ifndef _COMPAT_PCI_
#define _COMPAT_PCI_

#include_next <linux/pci.h>

#if defined(COMPAT_VMWARE)
static inline int pci_channel_offline(struct pci_dev *pdev)
{
	return 0;
}
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_PCI_ */
