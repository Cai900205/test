#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/reboot.h>

MODULE_LICENSE( "GPL");

#define GPIO_ADDR 0xffe130000
#define GPIO_MAPSIZE 0x3fff
#define FMAN1_MAC9 0xffe4f0000
#define FMAN1_MAC10 0xffe4f2000
#define FMAN2_MAC9 0xffe5f0000
#define FMAN2_MAC10 0xffe5f2000
#define EMAC_MAPSIZE 0x2000

static struct task_struct *test_task;

typedef struct T4_gpio
{
    uint32_t dir;
    uint32_t odr;
    uint32_t dat; 
    uint32_t ier; 
    uint32_t imr; 
    uint32_t icr; 
}t4_gpio;

typedef struct T4_mdio
{
    uint32_t cfg;
    uint32_t ctrl;
    uint32_t data;
    uint32_t addr;
}t4_mdio;

static void __iomem *gpio_addr=NULL;
static void __iomem *mac_addr[4]={NULL,NULL,NULL,NULL};
static uint32_t re_num[4]={0,0,0,0};

static void t4_gpio_setdirectionin(uint8_t gpio_number,uint8_t bitnumber)
{
    volatile t4_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (t4_gpio *)((char *)gpio_addr + gpio_number*0x1000);
    uint32_t dire=0;
    dire = im->dir;
    dire &= (~(1<<(31-bitnumber)));
    im->dir = dire;
}

static void t4_gpio_setdirectionout(uint8_t gpio_number,uint8_t bitnumber)
{
    volatile t4_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (t4_gpio *)((char *)gpio_addr + gpio_number*0x1000);
    uint32_t dire=0;
    dire = im->dir;
    dire |= (1<<(31-bitnumber));
    im->dir = dire;
}

static void t4_gpio_setbitlow(uint8_t gpio_number,uint8_t bitnumber)
{
    volatile t4_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (t4_gpio *)((char *)gpio_addr + gpio_number*0x1000);
    uint32_t data=0;
    data = im->dat;
    data &= (~(1<<(31-bitnumber)));
    im->dat = data;
}

static void t4_gpio_setbithigh(uint8_t gpio_number,uint8_t bitnumber)
{
    volatile t4_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (t4_gpio *)((char *)gpio_addr + gpio_number*0x1000);
    uint32_t data=0;
    data = im->dat;
    data |= (1<<(31-bitnumber));
    im->dat = data;
}

uint8_t t4_gpio_getbit(uint8_t gpio_number,uint8_t bitnumber)
{
    volatile t4_gpio *im = NULL;
    if(gpio_number>3 || bitnumber>31)
    {
        printk(KERN_WARNING "param error!...\n");
        return;
    }
    im = (t4_gpio *)((char *)gpio_addr + gpio_number*0x1000);
    uint32_t data=0;
    data = ((im->dat)&(1<<(31-bitnumber)));
    uint8_t value = (data>>(31-bitnumber));
    return value;
}

int gpio_set_value(uint8_t gpio_number,uint8_t bitnumber,int value)
{
    if(value)
    {
        t4_gpio_setdirectionout(gpio_number,bitnumber);
        t4_gpio_setbithigh(gpio_number,bitnumber);
    }
    else
    { 
        t4_gpio_setdirectionout(gpio_number,bitnumber);
        t4_gpio_setbitlow(gpio_number,bitnumber);
    }
    return 0;    
}
int gpio_get_value(uint8_t gpio_number,uint8_t bitnumber)
{
    int value;
    t4_gpio_setdirectionin(gpio_number,bitnumber);
    value = t4_gpio_getbit(gpio_number,bitnumber);
    return value;
}

static ssize_t IS_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int value=0;
    value=gpio_get_value(3,5); 
    gpio_set_value(3,5,value);
    return sprintf(buf,"%d\n",value);
}

static ssize_t IS_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,ssize_t  count)
{
    int value;
    sscanf(buf,"%d",&value);
    gpio_set_value(3,5,value); 
    if(count)
        return count;
    else
        return 1;
}
static ssize_t ATTN_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int value=0;
    value=gpio_get_value(3,4); 
    gpio_set_value(3,4,value);
    return sprintf(buf,"%d\n",value);
}

static ssize_t ATTN_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,ssize_t  count)
{
    int value;
    sscanf(buf,"%d",&value);
    gpio_set_value(3,4,value); 
    if(count)
        return count;
    else
        return 1;
}
static ssize_t ACT_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int value=0;
    value=gpio_get_value(3,6); 
    gpio_set_value(3,6,value);
    return sprintf(buf,"%d\n",value);
}

static ssize_t ACT_write(struct kobject *kobj, struct kobj_attribute *attr, const char *buf,ssize_t  count)
{
    int value;
    sscanf(buf,"%d",&value);
    gpio_set_value(3,6,value); 
    if(count)
        return count;
    else
        return 1;
}

static struct kobj_attribute IS =__ATTR(IS,0644, IS_read, IS_write);
static struct kobj_attribute ATTN =__ATTR(ATTN,0644, ATTN_read,ATTN_write);
static struct kobj_attribute ACT =__ATTR(ACT,0644, ACT_read,ACT_write);


static struct attribute *my_sysfs_test[] = {
    &IS.attr,
    &ATTN.attr,
    &ACT.attr,
    NULL,
};


static struct attribute_group my_attr_group = {
 .attrs = my_sysfs_test,
};

static int sysfs_status = 0 ;
struct kobject *soc_kobj = NULL;


static void board_active(void)
{
    gpio_set_value(1,26,0);
    gpio_set_value(3,5,1);
    gpio_set_value(3,6,1);
}
static void led_off(void)
{
    gpio_set_value(1,12,0);
    gpio_set_value(1,13,0);
    gpio_set_value(1,14,0);
    gpio_set_value(1,15,0);
    gpio_set_value(3,5,0);
    gpio_set_value(3,6,0);
}

static void led_flicker(int led)
{
    gpio_set_value(1,led,1);
    mdelay(200);
    gpio_set_value(1,led,0);
}
static void handle_pull(void)
{
    int value=0;
    value = gpio_get_value(1,27);
    if(!value)
    {
        mdelay(1);
        value = gpio_get_value(1,27);
        if(!value)
        {
            led_off();
            orderly_poweroff(true);
        }
    }
}

static void  mac_active(int fd,int led)
{
    uint32_t *num=(uint32_t *)(mac_addr[fd]+0x160);
    uint32_t rvl=0;
    rvl=*num - re_num[fd];
    if(rvl>0)
    {
       re_num[fd]=*num;
       led_flicker(led);         
    }
}

static void  mac_status(int fd,int led)
{
    int value=0;
    volatile t4_mdio *tmac=(t4_mdio *)(mac_addr[fd]+0x1030);
    int dir=0,gvalue=0;
    while(tmac->cfg & 0x80000000);
    tmac->ctrl=0x03;
    asm("sync");
    tmac->addr=0x20;  
    asm("sync");
    while(tmac->cfg & 0x80000000);
    tmac->ctrl=0x8003;
    asm("sync");
    while(tmac->cfg & 0x80000000);
    value=tmac->data;
    if(value&0x00001000)
    {
        mdelay(2);
        value=0;
        while(tmac->cfg & 0x80000000);
        tmac->ctrl=0x03;
        asm("sync");
        tmac->addr=0x20;  
        asm("sync");
        while(tmac->cfg & 0x80000000);
        tmac->ctrl=0x8003;
        asm("sync");
        while(tmac->cfg & 0x80000000);
        value=tmac->data;
        if(value & 0x00001000)
        {
            gpio_set_value(1,led,1);
            mac_active(fd,led);
        }
        else
        {
            gpio_set_value(1,led,0);
        }
    }
    else
    {
        gpio_set_value(1,led,0);
    }
}

static void my_function(void)
{
    board_active();
    while(!kthread_should_stop())
    {
        mac_status(0,15);
        mac_status(1,14);
        mac_status(2,12);
        mac_status(3,13);
        handle_pull();
        msleep(1000);
    }
    return;
}
 
static int kthread_app_init(void)
{
    int err=0,ret=0;
    gpio_addr=ioremap_nocache(GPIO_ADDR,GPIO_MAPSIZE);
    mac_addr[0]=ioremap_nocache(FMAN1_MAC9,EMAC_MAPSIZE);
    mac_addr[1]=ioremap_nocache(FMAN1_MAC10,EMAC_MAPSIZE);
    mac_addr[2]=ioremap_nocache(FMAN2_MAC9,EMAC_MAPSIZE);
    mac_addr[3]=ioremap_nocache(FMAN2_MAC10,EMAC_MAPSIZE);
    test_task = kthread_run((void *)my_function,NULL,"Test_task",1);
    if(IS_ERR(test_task))
    {
        printk("Unable to start kernel thread.\n");
        err= PTR_ERR(test_task);
        test_task=NULL;
        return err;
    }
    soc_kobj = kobject_create_and_add("front_led_op", NULL);
    if (!soc_kobj)
        goto err_board_obj;
    ret = sysfs_create_group(soc_kobj, &my_attr_group);
    if (ret)
        goto err_soc_sysfs_create;
    sysfs_status = 1;
    return  0;

    sysfs_status = 0;
err_soc_sysfs_create:
    sysfs_remove_group(soc_kobj, &my_attr_group);
    kobject_put(soc_kobj);
    printk("\nsysfs_create_group ERROR : %s\n",__func__);
    return 0;
err_board_obj:
    printk("\nobject_create_and_add ERROR : %s\n",__func__);
    return 0;

}
 
static void kthread_app_exit(void)
{
    led_off();
    if(!IS_ERR(test_task))
    {
        kthread_stop(test_task);
        test_task=NULL;
    }
    if(sysfs_status == 1)
    {
        sysfs_status = 0;
        sysfs_remove_group(soc_kobj, &my_attr_group);
        kobject_put(soc_kobj);
    }
    iounmap((void *)gpio_addr);
    iounmap((void *)mac_addr[0]);
    iounmap((void *)mac_addr[1]);
    iounmap((void *)mac_addr[2]);
    iounmap((void *)mac_addr[3]);
}
 
module_init(kthread_app_init);
module_exit(kthread_app_exit);
