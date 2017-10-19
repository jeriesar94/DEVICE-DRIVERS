#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "query_jeries_dev.h"

#define PCI_TIC_VENDOR 0x1337
#define PCI_TIC_DEVICE 0X0001
#define PCI_TIC_SUBVENDOR 0X1AF4
#define PCI_TIC_SUBDEVICE 0X1100
#define MAX_TIC_MAPS 1
#define MAX_TIC_PORT_REGIONS 1
#define FIRST_MINOR 0
#define MINOR_CNT 1

static struct pci_driver jeries_tic;
static struct pci_dev *jdev;
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;
struct tic_mem{
	const char *name;
	void __iomem *start;
	unsigned long size;
};

struct tic_io{
	const char *name;
	unsigned long start;
	unsigned long size;
};

struct tic_info{
	struct tic_mem mem[MAX_TIC_MAPS];
	struct tic_io port[MAX_TIC_PORT_REGIONS];
	u8 irq;
};




/*Function to handle IOCTL commands given from userspace
Implementation:
Swtich case checks command as follows:
JERIES_DEV_ALERT:	Send an interrupt signal to the device, then write the value of 1 to mmio addr:offset=0, this will set the awake register to 1 (waking up 				the device).
JERIES_DEV_GET_STATE:	Read mmio at addr:offset = 0 to get the awake register, then check its value to determine if the device is awake or asleep.
JERIES_DEV_SLEEP:	Read mmio at addr:offset = 0 to check if the device is already asleep, otherwise write the value of 0 to addr:offset=0 and put the device to 				sleep.

If any other command is sent, the default statement is invoked informing the user that no action is to be taken.
*/
static long device_ioctl(struct file *f, unsigned int cmd, unsigned long args){
	struct tic_info *info;
	//Get tic_info pointer corresponding to the desired device.
	info = pci_get_drvdata(jdev);
	switch(cmd){
		case JERIES_DEV_ALERT:
			iowrite16(1, info->mem[0].start);
			pr_alert("Device is Alert\n");
			break;	
		case JERIES_DEV_GET_STATE:
			if(ioread16(info->mem[0].start)){
				pr_alert("Device awake\n");
			}
			else{
				pr_alert("Device asleep\n");				
			}
			break;
		case JERIES_DEV_SLEEP:
			if(!ioread16(info->mem[0].start)){
				printk("Device is already asleep\n");
			}
			else{
				outb(1, info->port[0].start);
				iowrite16(0, info->mem[0].start);
				pr_alert("Device went to sleep\n");			
			}
			break;
		case JERIES_DEV_WRITE:
			outl(args, (info->port[0].start)+4);
			break;
		case JERIES_DEV_READ:
			printk("ARGS: %d\n", args);
			if(args != 0){
				printk("%c \n", inb(info->port[0].start+4));
			}
			else{
				outl(0, info->port[0].start+8);
			}
			break;
		default: pr_alert("Not taking any action");
			
	}
	//set new information to driver related with the device.
	pci_set_drvdata(jdev, info);
	return 0;
}
/*
Function that handles interrupts sent by the device.
*/
static irqreturn_t jeries_tic_handler(int irq, void *dev_info){
	struct tic_info *info = (struct tic_info *) dev_info;
	pr_alert("IRQ %d handled\n", irq);
	if(inl(info->port[0].start)){
		outl(0, info->port[0].start);
		return IRQ_HANDLED;
	}
	else{
		return IRQ_NONE;
	}
}
/*
query_fops: structure that describes the file operations corresponding with device's device file. 
.owner:			Owner of the device file.
.ioctl:			Function responsible for handling IOCTL requests from userspace.
.unlocked_ioctl:	same as .ioctl but for kernel version > KERNEL_VERSION(2,6,35)
*/
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = device_ioctl
#else
    .unlocked_ioctl = device_ioctl
#endif
};

/*IOCTL intializattion function, which creates device file with it's correspoding major and minor numbers*/
static void __init query_ioctl_init(void)
{
    int ret;
    int major;
    struct device *dev_ret;
 
 
    printk("trying to register_chrdev\n");
    if((major = register_chrdev(0, "jeries_ioctl", &query_fops)) < 0){
	printk("Cannot Register Device\n");
	return major;

    }
    printk("MAJOR IS=%d \n", major);
    printk("Creating device class...\n");
    if (IS_ERR(cl = class_create(THIS_MODULE, "jeriesDEV")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
	dev = MKDEV(major, 0);
    printk("Creating Device\n");
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "jerdev")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
    return 0;
}

/*Function to handle the deletion of the device file, and ending all ioctl relations*/
static void __exit query_ioctl_exit(void)
{
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}
/*This function is invoked when the module is inserted
What happens in this function is as follows:
1- Memory is allocated to store information about the device's driver.
2- Device is enabled on the bus.
3- Device MMIO and PIO memory regions are requested. 
4- Configure driver to recognize memory regions related to MMIO and PIO.
5- request an interrupt number and register it with our device.
6- set driver information corresponding to this device.
7- Initialize IOCTL
*/
static int jeries_tic_probe(struct pci_dev *dev, const struct pci_device_id *id){
	struct tic_info *info;
	//Allocate memory to store information.
	info = kzalloc(sizeof(struct tic_info), GFP_KERNEL);
	if(!info){
		return -ENOMEM;
	}
	//Enable device on bus
	if(pci_enable_device(dev)){
		goto out_free;
	}
	pr_alert("Enabled Device\n");
	//Request MMIO and IO Regions
	if(pci_request_regions(dev, "jeries_tic")){
		goto out_disable;
	}
	
	pr_alert("Requested Regions\n");
	//Configure driver to recognize regions related with MMIO and PIO
	info->port[0].name = "jeries_io";
	info->port[0].start = pci_resource_start(dev, 0);
	info->port[0].size = (pci_resource_len(dev, 0));

	info->mem[0].name = "jeries_mmio";
	info->mem[0].start = pci_ioremap_bar(dev, 1);
	info->mem[0].size = pci_resource_len(dev, 1);
	if(!info->mem[0].start){
		goto out_unrequest;
	}
	
	pr_alert("Addresses are remapped for kernel uses\n");
	//Request an interrupt number and register it with the device.
	if(pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &info->irq)){
		goto out_iounmap;
	}

	if(devm_request_irq(&dev->dev, info->irq, jeries_tic_handler, IRQF_SHARED, jeries_tic.name, (void *) info)){
		goto out_iounmap;
	}
	//Send an interrupt and alert the device.
	//outb(1, info->port[0].start);
	iowrite8(1, info->mem[0].start);
	//set driver information corresponding to this device.
	jdev = dev;
	pci_set_drvdata(dev, info);
	//Initialize IOCTL
	query_ioctl_init();
	return 0;
	/*MACROS to handle any failures that occur during the process*/
	out_iounmap:
		pr_alert("tic:probe_out:iounmap\n");
		iounmap(info->mem[0].start);
	out_unrequest:
		pr_alert("tic:probe_out_unrequest\n");
		pci_release_regions(dev);
	out_disable:
		pr_alert("tic:probe_out_disable\n");
		pci_disable_device(dev);
	out_free:
		pr_alert("tic:probe_out_free\n");
		kfree(info);
		return -ENODEV;
}
/*This function removes all driver information and frees rescources when this module is removed from the kernel*/
static void jeries_tic_remove(struct pci_dev *dev){
	struct tic_info *info = pci_get_drvdata(dev);
	pci_release_regions(dev);
	pci_disable_device(dev);
	iounmap(info->mem[0].start);
	kfree(info);
}
/*PCI DEVICE IDs Table*/
static struct pci_device_id jeries_tic_ids[] = {
	{PCI_DEVICE(PCI_TIC_VENDOR, PCI_TIC_DEVICE)},
	{0,}
};

/*
jeries_tic: Structure that contains information about the driver to be understood by the PCI BUS, such as it's name, the IDs of the device related to this driver, module initial and final functions.

.name:		Name of the driver.
.id_table:	IDs of the device handled by this driver.
.probe:		Function invoked when the module is inserted to the kernel.
.remove:	Function invoked when the module is removed from the kernel.
*/
static struct pci_driver jeries_tic = {
	.name = "jeries_tic",
	.id_table = jeries_tic_ids,
	.probe = jeries_tic_probe,
	.remove = jeries_tic_remove,
};

/*Kernel module information*/
module_pci_driver(jeries_tic);
MODULE_DEVICE_TABLE(pci, jeries_tic_ids);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This is Jeries's Driver...");
MODULE_AUTHOR("Jeries Abedrabbo");
