#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h> 
#include <linux/timer.h> 
#include <linux/kthread.h>
#include <linux/delay.h>
#include<linux/device.h>
#include <linux/cdev.h>
 
#ifndef VM_RESERVED
	#define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define READ_IOCTL _IOR('G', 0, int)
#define WRITE_IOCTL _IOW('G', 1, int)
 
#define TIME_MS 500

static dev_t first;
static struct class *cl;
static struct cdev c_dev;
struct mmap_info *address;
static struct timer_list my_timer;
struct dentry  *my_file;
int out;
char buf[200];

struct mmap_info
{
	char *data;            
 	int reference;      
};

void increment_value(void)
{
	unsigned long value;
	value = *(address->data);
	value++;
	memcpy(address->data, &value, sizeof(unsigned long));	
}

void timer_callback(unsigned long data)
{
	if(out)
	{	
		printk("No more timer\n");
		return;
	}
	/* Restart Timer */ 
	del_timer(&my_timer);
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIME_MS));
	
	increment_value();
	return;
}

void mmap_open(struct vm_area_struct *vma)
{
    struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    info->reference++;
	printk("Mmap Open\n");
}
 
void mmap_close(struct vm_area_struct *vma)
{
    struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    info->reference--;
	printk("Mmap Close\n");
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct mmap_info *info;    
 
	info = (struct mmap_info *)vma->vm_private_data;
	if (!info->data)
	{
		printk("No data\n");
		return 0;    
	}
 
	vmf->page = virt_to_page(info->data);    
	get_page(vmf->page);            

	printk("Mmap Fault\n");
	return 0;
}

struct vm_operations_struct mmap_vm_ops =
{
	.open =     mmap_open,
	.close =    mmap_close,
	.fault =    mmap_fault,    
};
 
int op_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;    
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

 int mmapfop_close(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;
     
	free_page((unsigned long)info->data);
	kfree(info);
	filp->private_data = NULL;
	printk("Release is called\n");
	return 0;
}

int mmapfop_open(struct inode *inode, struct file *filp)
{
	address = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);   
	if(address == NULL)
	{
		printk("cannot allocate memory\n");
		return -1;
	}
	
	address->data = (char *)get_zeroed_page(GFP_KERNEL);
    filp->private_data = address;
	return 0;
}

static const struct file_operations mmap_fops = {
    .open = mmapfop_open,
	.release = mmapfop_close,
    .mmap = op_mmap,
};

static int ioctl_open( struct inode *inode, struct file *file )
{
    return 0;
}
 
static int ioctl_close( struct inode *inode, struct file *file )
{
    return 0;
}

/*Receive Commands from user*/
static long ioctl_command(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int len = 200;
	switch(cmd) 
	{
		case READ_IOCTL:	
			copy_to_user((char *)arg, buf, 200);
			break;
		
		case WRITE_IOCTL:
			copy_from_user(buf, (char *)arg, len);
			if(buf[0] == '1')
			{
				my_file = debugfs_create_file("memory_map_fs", 0777, NULL, NULL, &mmap_fops);
				if(my_file == NULL)
					printk("Error creating file\n");
			}
			else if(buf[0] == '2')
			{			
				out = 0;
				setup_timer(&my_timer, timer_callback, 0);
				increment_value();
				mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIME_MS));//1st timer start
				printk("First Timer started\n");
			}
			if(buf[0] == '0')
			{
				out = 1;
				del_timer(&my_timer);
				printk("Last Timer Deleted\n");
				debugfs_remove(my_file);
			}
			break;

		default:
			return -ENOTTY;
	}
	return len;
}

static struct file_operations file_func = {
	.owner = THIS_MODULE,
	.open = ioctl_open,
	.release = ioctl_close,
	.unlocked_ioctl = ioctl_command,
};

/*init module*/
static int __init map_module_init(void)
{	
    if (alloc_chrdev_region(&first, 0, 1, "my_char_dev") < 0)
    {
        return -1;
    }
	
    if ((cl = class_create(THIS_MODULE, "my_chardrv")) == NULL)
    {
        unregister_chrdev_region(first, 1);
        return -1;
    }
	
    if (device_create(cl, NULL, first, NULL, "my_char_device") == NULL)
    {
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }
	
    cdev_init(&c_dev, &file_func);
    if (cdev_add(&c_dev, first, 1) == -1)
    {
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        return -1;
    }
	
 	return 0;
}

/*Exit module*/
static void __exit map_module_exit(void)
{
	cdev_del(&c_dev);
	device_destroy(cl, first);
    class_destroy(cl);
	unregister_chrdev_region(first, 1);
	debugfs_remove(my_file);
	del_timer(&my_timer);				
}  

module_init(map_module_init);
module_exit(map_module_exit);
MODULE_LICENSE("GPL");
