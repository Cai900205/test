#ifndef GENERIC_PCIE_DRIVER_H
#define GENERIC_PCIE_DRIVER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/version.h>



#define GENERIC_PCIE_VENDORID 0x10EE
#define GENERIC_PCIE_DEVICEID 0x7024




struct generic_dev
{
    struct cdev cdev;
    struct pci_dev * pdev;
    dev_t first;

    struct st_cfg
    {
        u32 slot;
        u32 func;
        u32 irq;
        #define PCI_TYPE0_ADDRESSES     6
        u32     typ[PCI_TYPE0_ADDRESSES];
        u64     bar[PCI_TYPE0_ADDRESSES];
        void    *map[PCI_TYPE0_ADDRESSES];
        u32     siz[PCI_TYPE0_ADDRESSES];
        u8      using_base_num;
    }  cfg;
};


/** Trace flag. */
#define TR_F_OPEN					(1 << 1) 
#define TR_F_CLOSE					(1 << 2)
#define TR_F_IOCTL					(1 << 3)
#define TR_F_ERROR					(1 << 4)
#define TR_F_REMOVE					(1 << 5)
#define TR_F_MMAP					(1 << 6)


extern int global_trace; //(TR_F_OPEN || TR_F_CLOSE || TR_F_IOCTL); 

#define TRACE(trace,fmt, ...)                                            	\
        do {                                                                \
            if ((global_trace & (trace)) == TR_F_OPEN)            			\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
            else if ((global_trace & (trace)) == TR_F_CLOSE)            	\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
            else if ((global_trace & (trace)) == TR_F_IOCTL)            	\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
            else if ((global_trace & (trace)) == TR_F_ERROR)            	\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
            else if ((global_trace & (trace)) == TR_F_REMOVE)            	\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
            else if ((global_trace & (trace)) == TR_F_MMAP)            		\
                printk("<%s> "fmt"", __FUNCTION__, ##__VA_ARGS__); 			\
        } while(0)


#endif
