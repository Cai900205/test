#ifndef _COMPAT_DEVICE_H_
#define _COMPAT_DEVICE_H_

#if defined(COMPAT_VMWARE)
# include <compat/vmkl-device-rename.h>
#endif

#include_next <linux/device.h>

#if defined(COMPAT_VMWARE)
/* Horrible voodoo - include all device users before switching back to no mapping mode. */
#include <linux/pci.h>
#include <linux/usb.h>
#include <linux/dmaengine.h>
#include <scsi/scsi_device.h>

/* Define vmkl version of the device logging macro. Copied from vmklinux in
 * ESXi 5.5 */

/**
 *  dev_printk - A wrapper for printk(), that prints driver identification and
 *  device location information
 *  @level: log level
 *  @dev: pointer to device struct
 *  @format: printk style format string
 *
 *  A wrapper for printk(), that prints driver identification and
 *  device location information
 *
 *  SYNOPSIS:
 *      #define dev_printk(level, dev, format, arg...)
 *
 *  RETURN VALUE:
 *  NONE 
 *
 */
/* _VMKLNX_CODECHECK_: dev_printk */
#define vmkl_dev_printk(level, dev, format, arg...)  \
        printk(level "%s %s: " format , dev_driver_string(dev) , (dev)->bus_id , ## arg)

# include <compat/vmkl-device-unrename.h>
#endif /* COMPAT_VMWARE */

#endif /* _COMPAT_DEVICE_H_ */
