#ifndef _COMPAT_DMA_MAPPING_
#define _COMPAT_DMA_MAPPING_

#include_next <linux/dma-mapping.h>

#if defined(COMPAT_VMWARE)
#include <linux/pci.h>
#endif

#endif /* _COMPAT_DMA_MAPPING_ */
