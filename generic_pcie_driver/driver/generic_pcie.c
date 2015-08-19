/*************************************************************************
	> File Name: generic_pcie.c
	> Author: 
	> Mail: 
	> Created Time: Wed 19 Nov 2014 04:16:55 PM CST
 ************************************************************************/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include "../include/common.h"
#include "generic_pcie.h"
#define GENERIC_PCIE_NAME "generic_pcie_device"
#define GENERIC_PCIE_NUMS 1
struct generic_dev gdev_body;
int global_trace = TR_F_ERROR;
static struct pci_device_id pcie_generic_table[] __devinitdata = 
{
    {
        .vendor = GENERIC_PCIE_VENDORID,
        .device = GENERIC_PCIE_DEVICEID,
        .subvendor = PCI_ANY_ID,
        .subdevice = PCI_ANY_ID,
    },
    { /* all zeros */ }
};
MODULE_DEVICE_TABLE(pci, pcie_generic_table);
static inline void reg_read_ullong(struct generic_dev* gdev, 
                                    u8 bar,
								    u32 offset,
								    u64* data)
{
	*data = readq((u64*)(gdev->cfg.map[bar] + offset));
}
static inline void reg_write_ullong(struct generic_dev* gdev,
									 u8 bar,
									 u32 offset,
									 u64 data)
{
	writeq(data, (u64*)(gdev->cfg.map[bar] + offset));
}
static inline void reg_write_ulong(struct generic_dev* ldev,
                                    u8 bar,
                                    u32 offset,
                                    u32 data )
{
    writel(data, (u32*)(ldev->cfg.map[bar] + offset));
}
static inline void reg_read_ulong(struct generic_dev* ldev,
                                   u8 bar,
                                   u32 offset,
                                   u32* data )
{
    *data = readl((u32*)(ldev->cfg.map[bar] + offset));
}
static int generic_pcie_read_ulong(struct generic_dev * gdev, unsigned long arg)
{
    int result = -1;
    iomsg_t msg;
    u32 data = 0;
    /** Copy the message from user stack. */
    result = access_ok(VERIFY_WRITE, (void*)arg, sizeof(iomsg_t));
    if (!result)
    {
                TRACE(TR_F_ERROR, "Access to the message from user stack is invalid.\n");
                return -1;
    }
    result = copy_from_user(&msg, (void*)arg, sizeof(iomsg_t));
    if (result != 0)
    {
                TRACE(TR_F_ERROR, "Try to copy the message from user stack failed.\n");
                return -1;
    }
    /** Read the register here. */

    reg_read_ulong(gdev, msg.bar, msg.offset, &data);
    msg.data = data;
    result = copy_to_user((void*)arg, &msg, sizeof(iomsg_t));
    if (result != 0)
    {
        TRACE(TR_F_ERROR, "Return value of the register failed.\n");
        return -1;
    }
    return 0;
}
static int generic_pcie_read_ullong(struct generic_dev * gdev, unsigned long arg)
{
    int result = -1;
    iomsg_t msg;
    /** Copy the message from user stack. */
    result = access_ok(VERIFY_WRITE, (void*)arg, sizeof(iomsg_t));
    if (!result)
    {
                TRACE(TR_F_ERROR, "Access to the message from user stack is invalid.\n");
                return -1;
    }
    result = copy_from_user(&msg, (void*)arg, sizeof(iomsg_t));
    if (result != 0)
    {
                TRACE(TR_F_ERROR, "Try to copy the message from user stack failed.\n");
                return -1;
    }
    /** Read the register here. */
    reg_read_ullong(gdev, msg.bar, msg.offset, &msg.data);
    result = copy_to_user((void*)arg, &msg, sizeof(iomsg_t));
    if (result != 0)
    {
        TRACE(TR_F_ERROR, "Return value of the register failed.\n");
        return -1;
    }
    return 0;
}
static int generic_pcie_write_ulong(struct generic_dev * gdev, unsigned long arg)
{
	int result = -1;
	iomsg_t msg;
    u32 data = 0;
	/** Copy the message from user stack. */
	result = access_ok(VERIFY_WRITE, (void*)arg, sizeof(iomsg_t));
	if (!result)
	{
		TRACE(TR_F_ERROR, "Access to the message from user stack is invalid.\n");
		return -1;
	}
	result = copy_from_user(&msg, (void*)arg, sizeof(iomsg_t));
	if (result != 0)
	{
		TRACE(TR_F_ERROR, "Try to copy the message from user stack failed.\n");
		return -1;
	}
	/** [ISSUE]: We should check the alignment here. */
    data = msg.data;
	reg_write_ulong(gdev, msg.bar, msg.offset, data);
	result = copy_to_user((void*)arg, &msg, sizeof(iomsg_t));
	if (result != 0)
	{
		TRACE(TR_F_ERROR, "Return value of the register failed.\n");
		return -1;
	}
	return 0;
}
static int generic_pcie_write_ullong(struct generic_dev * gdev, unsigned long arg)
{
	int result = -1;
	iomsg_t msg;
	/** Copy the message from user stack. */
	result = access_ok(VERIFY_WRITE, (void*)arg, sizeof(iomsg_t));
	if (!result)
	{
		TRACE(TR_F_ERROR, "Access to the message from user stack is invalid.\n");
		return -1;
	}
	result = copy_from_user(&msg, (void*)arg, sizeof(iomsg_t));
	if (result != 0)
	{
		TRACE(TR_F_ERROR, "Try to copy the message from user stack failed.\n");
		return -1;
	}
	/** [ISSUE]: We should check the alignment here. */
	reg_write_ullong(gdev, msg.bar, msg.offset, msg.data);
	result = copy_to_user((void*)arg, &msg, sizeof(iomsg_t));
	if (result != 0)
	{
		TRACE(TR_F_ERROR, "Return value of the register failed.\n");
		return -1;
	}
	return 0;
}
static int generic_pcie_open(struct inode * inode, struct file * filp)
{
    struct generic_dev * gdev = &gdev_body;
    filp->private_data = gdev;
    return 0;
}
static int generic_pcie_close(struct inode * inode, struct file * filp)
{
    return 0;
}
static long generic_pcie_ioctl(struct file * fp, unsigned int cmd, unsigned long arg)
{
    int stat = -1;
    struct generic_dev * gdev = &gdev_body;
    if(_IOC_TYPE(cmd) != GENERIC_PCIE_TYPE)
    {
        TRACE(TR_F_ERROR, "The ioctl type is not supported.\n");
        return -1;
    }
    switch(cmd)
    {
        case IOCTL_READ_ULLONG:
        stat = generic_pcie_read_ullong(gdev,arg);
        break;
        case IOCTL_WRITE_ULLONG:
        stat = generic_pcie_write_ullong(gdev,arg);
        break;
        case IOCTL_READ_ULONG:
        stat = generic_pcie_read_ulong(gdev,arg);
        break;
        case IOCTL_WRITE_ULONG:
        stat = generic_pcie_write_ulong(gdev,arg);
        break;
        default:
        /** Just notify the unsupported command. */
        TRACE(TR_F_ERROR, "Unsupported command.\n");
        return -1;
    }
    return stat;
}
static const struct file_operations generic_pcie_ops = 
{
    .owner            = THIS_MODULE,
    .open             = generic_pcie_open,
    .release          = generic_pcie_close,
    .unlocked_ioctl   = generic_pcie_ioctl,
};
static int pcie_generic_probe(struct pci_dev * pci_dev, const struct pci_device_id * id)
{
    int result = -1;
    int i      = 0;
    struct generic_dev * gdev = &gdev_body;
    /** Enable the device. */
    result = pci_enable_device(pci_dev);
    if (result)
    {
        TRACE(TR_F_ERROR, "Enable the pcie device failed.\n");
        return result;
    }
    pci_set_master(pci_dev);
    result = pci_request_regions(pci_dev, GENERIC_PCIE_NAME);
    if (result)
    {
        TRACE(TR_F_ERROR, "Request the pcie regions failed.\n");
        return result;
    }
    memset(gdev,0,sizeof(struct generic_dev));
    /* Get PCIe regions. */
    for(i = 0; i < 6; i++)
    {
		if (pci_resource_flags(pci_dev, i) == 0)
		{
			break;
		}
		gdev->cfg.bar[i] = pci_resource_start(pci_dev, i);
		gdev->cfg.siz[i] = pci_resource_len(pci_dev, i);
		if (pci_resource_flags(pci_dev, i) & IORESOURCE_IO)
		{
			gdev->cfg.typ[i] = IORESOURCE_IO;
			gdev->cfg.map[i] = (u8*)pci_resource_start(pci_dev, i);
		}
		else if (pci_resource_flags(pci_dev, i) & IORESOURCE_MEM)
		{
			gdev->cfg.typ[i] = IORESOURCE_MEM;
			gdev->cfg.map[i] = (u8*)ioremap_nocache(gdev->cfg.bar[i], 
												   gdev->cfg.siz[i]);
		}
    }
	/** Setup the character device. */
	result = alloc_chrdev_region(&gdev->first, 0, 
								 GENERIC_PCIE_NUMS, 
								 GENERIC_PCIE_NAME);
	if (result != 0)
	{
		TRACE(TR_F_ERROR, "Allocate the region for character device failed.\n");
		goto alloc_cregion_err;
	}
	cdev_init(&gdev->cdev, &generic_pcie_ops);
	gdev->cdev.owner = THIS_MODULE;
	result = cdev_add(&gdev->cdev, gdev->first, GENERIC_PCIE_NUMS);
	if (result != 0)	
	{
		TRACE(TR_F_ERROR, "Add the character driver failed.\n");
		goto add_cdev_err;
	}
	gdev->cfg.slot = PCI_SLOT(pci_dev->devfn);
	gdev->cfg.func = PCI_FUNC(pci_dev->devfn);
	gdev->pdev = pci_dev;
	pci_set_drvdata(pci_dev, gdev);
	return 0;
add_cdev_err:
	unregister_chrdev_region(gdev->first, GENERIC_PCIE_NUMS);
alloc_cregion_err:
	/** Unmap the I/O memory. */
	for (i = 0; i < PCI_TYPE0_ADDRESSES; i++)
	{
		if (gdev->cfg.typ[i] == IORESOURCE_MEM && gdev->cfg.map[i])
		{
			iounmap((void*)gdev->cfg.map[i]);
		}
	}
	pci_release_regions(pci_dev);
	return -1;
}
static void pcie_generic_remove(struct pci_dev * dev)
{
    int i = 0;
    struct generic_dev * gdev = &gdev_body;
	/** Unmap the I/O memory. */
	for (i = 0; i < PCI_TYPE0_ADDRESSES; i++)
	{
		if (gdev->cfg.typ[i] == IORESOURCE_MEM && gdev->cfg.map[i])
		{
			iounmap((void*)gdev->cfg.map[i]);
		}
	}
    TRACE(TR_F_REMOVE, "Unmap the I/O memory of the PCIE success.\n");
    pci_release_regions(dev);
    pci_set_drvdata(dev, NULL);
}
static struct pci_driver pcie_generic_driver =
{
    .name       = GENERIC_PCIE_NAME,
    .id_table   = pcie_generic_table,
    .probe      = pcie_generic_probe,
    .remove     = pcie_generic_remove,
};
static int __init generic_init(void)
{
    int result = -1;
    result = pci_register_driver(&pcie_generic_driver);
    if (result)
    {
                TRACE(TR_F_ERROR, "Register the pcie driver for ladbrd failed.\n");
                return result;
    }
    return 0;
}
static void __exit generic_exit(void)
{
    struct generic_dev * gdev = &gdev_body;
    unregister_chrdev_region(gdev->first, 1);
    cdev_del(&gdev->cdev);
    pci_unregister_driver(&pcie_generic_driver);
}
module_init(generic_init);
module_exit(generic_exit);  
MODULE_LICENSE("GPL");
