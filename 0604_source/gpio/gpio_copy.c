#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#define GPIO_MODULE_MAJOR    240
#define GPIO_MODULE_NAME     "T4240_gpio"
#define MEM_SIZE             0x200000
#define T4240_gpio_ADDR    0xffe130000
#define T4240_gpio_MAPSIZE 0x3fff
#define INPUT 0
#define SET  1
#define CLEAR  2
#define TOGGLE     3

struct T4240_gpio_dev
{
	struct cdev cdev;
	u8 mem[MEM_SIZE];
};
struct gpio_param
{
    u8 gpionum;
    u8 bitnum;
    u8 value;
};

struct T4240_gpio_dev *T4240_gpio_devp = NULL;

typedef struct t4240_gpio
{
	u32 dir;		/* direction register */
	u32 odr;		/* open drain register */
	u32 dat;		/* data register */
	u32 ier;		/* interrupt event register */
	u32 imr;		/* interrupt mask register */
	u32 icr;		/* external interrupt control register */
/*	u8 res0[0xe8];*/
}T4240_gpio;


/*static unsigned int T4240_gpio_addr = 0;*/

static void __iomem * T4240_gpio_addr = 0;

static void T4240_gpio_setdirectionin(u8 gpio_number,u8 bitnumber)
{
    volatile T4240_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (T4240_gpio *)((char *)T4240_gpio_addr + gpio_number*0x1000);
    u32 direction = 0;
    direction = im->dir;
    direction &= (~(1<<(31-bitnumber)));
    im->dir = direction;
}

static void T4240_gpio_setdirectionout(u8 gpio_number,u8 bitnumber)
{
    volatile T4240_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (T4240_gpio *)((char *)T4240_gpio_addr + gpio_number*0x1000);
    u32 direction = 0;
    direction = im->dir;
    direction |= (1<<(31-bitnumber));
    im->dir = direction;
}

static void T4240_gpio_setbitlow(u8 gpio_number,u8 bitnumber)
{
    volatile T4240_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (T4240_gpio *)((char *)T4240_gpio_addr + gpio_number*0x1000);
    u32 gpiodata = 0;
    gpiodata = im->dat;
    gpiodata &= (~(1<<(31-bitnumber)));
    im->dat = gpiodata;
}

static void T4240_gpio_setbithigh(u8 gpio_number,u8 bitnumber)
{
    volatile T4240_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (T4240_gpio *)((char *)T4240_gpio_addr + gpio_number*0x1000);
    u32 gpiodata = 0;
    gpiodata = im->dat;
    gpiodata |= (1<<(31-bitnumber));
    im->dat = gpiodata;
}

u8 T4240_gpio_getbit(u8 gpio_number,u8 bitnumber)
{
    volatile T4240_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (T4240_gpio *)((char *)T4240_gpio_addr + gpio_number*0x1000);
    u32 gpiodata = 0;
    gpiodata = ((im->dat)&(1<<(31-bitnumber)));
    u8 value = (gpiodata>>(31-bitnumber));
    return value;
}

int T4240_gpio_open(struct inode *inode,struct file *filp)
{
    filp->private_data = T4240_gpio_devp;
    return 0;
}
int T4240_gpio_release(struct inode *inode,struct file *filp)
{
    filp->private_data = NULL;   
    return 0;
}
int gpio_direction_input(u8 gpio_number,u8 bitnumber)
{
    T4240_gpio_setdirectionin(gpio_number,bitnumber);

    return 0;
}
int gpio_direction_output(u8 gpio_number,u8 bitnumber,int value)
{
    if(value)
    {
        T4240_gpio_setdirectionout(gpio_number,bitnumber);
        T4240_gpio_setbithigh(gpio_number,bitnumber);
    }
    else
    { 
        T4240_gpio_setdirectionout(gpio_number,bitnumber);
        T4240_gpio_setbitlow(gpio_number,bitnumber);
    }
    return 0;    
}
int gpio_set_value(u8 gpio_number,u8 bitnumber,int value)
{
    if(value)
    {
        T4240_gpio_setdirectionout(gpio_number,bitnumber);
        T4240_gpio_setbithigh(gpio_number,bitnumber);
    }
    else
    { 
        T4240_gpio_setdirectionout(gpio_number,bitnumber);
        T4240_gpio_setbitlow(gpio_number,bitnumber);
    }
    return 0;    
}
int gpio_get_value(u8 gpio_number,u8 bitnumber)
{
    int value;
    T4240_gpio_setdirectionin(gpio_number,bitnumber);
    value = T4240_gpio_getbit(gpio_number,bitnumber);
    return value;
}

long T4240_gpio_ioctl(struct file *filp,unsigned int cmd,struct gpio_param *buf)
{
        u8 value;
        struct T4240_gpio_dev *dev = filp->private_data;
        struct gpio_param *gpio = dev->mem;
    
        if(copy_from_user(gpio,buf,sizeof(struct gpio_param)))
        {
            return -1;
        }
	switch(cmd)
	{
	case INPUT:
            value=gpio_get_value(gpio->gpionum,gpio->bitnum);
            if(copy_to_user(&buf->value,&value,sizeof(u8)))
            {
                return -1;
	    }
	    break;
        case SET:
            gpio_set_value(gpio->gpionum,gpio->bitnum,1);
            break;
        case CLEAR:
            gpio_set_value(gpio->gpionum,gpio->bitnum,0);
            break;
        case TOGGLE:
            value = !gpio_get_value(gpio->gpionum,gpio->bitnum);
            if(value)
            gpio_set_value(gpio->gpionum,gpio->bitnum,1);
            else
            gpio_set_value(gpio->gpionum,gpio->bitnum,0);

            break;
	}
	return 0;
}

static struct file_operations T4240_gpio_fops=
{
	.owner	  =	THIS_MODULE,
	.open	  =	T4240_gpio_open,
	.release  =     T4240_gpio_release,
	.unlocked_ioctl	  =	T4240_gpio_ioctl,
};

static void T4240_gpio_setup_cdev(struct T4240_gpio_dev *dev,int index)
{
	int err = -1;
        int devno = MKDEV(GPIO_MODULE_MAJOR,index);
	cdev_init(&dev->cdev,&T4240_gpio_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
	{
		printk(KERN_WARNING "Error %d adding T4240 gpio %d\n",err,index);
	}
}

static int __init T4240_gpio_init(void)
{
	int ret = -1;
	dev_t devno;

	T4240_gpio_addr = ioremap_nocache(T4240_gpio_ADDR,T4240_gpio_MAPSIZE);
	if(T4240_gpio_addr == 0)
	{
		printk(KERN_WARNING "failed to ioremap\n");
		return -EIO;
	}

	devno = MKDEV(GPIO_MODULE_MAJOR,0);
	ret = register_chrdev_region(devno,1,GPIO_MODULE_NAME);
	if(ret != 0)
	{
		printk(KERN_WARNING "%s:can't get major %d\n",GPIO_MODULE_NAME,GPIO_MODULE_MAJOR);
		goto iounmap_gpio;
	}

	T4240_gpio_devp = kmalloc(sizeof(struct T4240_gpio_dev),GFP_KERNEL);
	if(T4240_gpio_devp == NULL)
	{
		goto unregister_chrdev;
	}else

	memset(T4240_gpio_devp,0,sizeof(struct T4240_gpio_dev));
	T4240_gpio_setup_cdev(T4240_gpio_devp,0);
	goto out;

unregister_chrdev:
	unregister_chrdev_region(devno,1);
iounmap_gpio:
	iounmap ((void *)T4240_gpio_addr);
	T4240_gpio_addr = 0;
	return -1;
out:
	return 0;
}

static void __exit T4240_gpio_exit(void)
{
	if(T4240_gpio_addr)
	{
		iounmap((void *)T4240_gpio_addr);
		T4240_gpio_addr = 0;
	}

	cdev_del(&T4240_gpio_devp->cdev);
	kfree(T4240_gpio_devp);
	T4240_gpio_devp = NULL;
	unregister_chrdev_region(MKDEV(GPIO_MODULE_MAJOR,0),1);
}

module_init(T4240_gpio_init);
module_exit(T4240_gpio_exit);
