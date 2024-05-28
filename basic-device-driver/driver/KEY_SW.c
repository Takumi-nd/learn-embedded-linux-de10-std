#include <linux/fs.h>           // struct file, struct file_operations
#include <linux/init.h>         // for __init, see code
#include <linux/module.h>       // for module init and exit macros
#include <linux/miscdevice.h>   // for misc_device_register and struct miscdev
#include <linux/uaccess.h>      // for copy_to_user, see code
#include <asm/io.h>             // for mmap
#include "../../inc/address_map_arm.h"
#include "../../inc/interrupt_ID.h"

static int key_device_open (struct inode *, struct file *);
static int key_device_release (struct inode *, struct file *);
static ssize_t key_device_read (struct file *, char *, size_t, loff_t *);

static int sw_device_open (struct inode *, struct file *);
static int sw_device_release (struct inode *, struct file *);
static ssize_t sw_device_read (struct file *, char *, size_t, loff_t *);

static struct file_operations keydev_fops = {
    .owner = THIS_MODULE,
    .read = key_device_read,
    .open = key_device_open,
    .release = key_device_release
};

static struct file_operations swdev_fops = {
    .owner = THIS_MODULE,
    .read = sw_device_read,
    .open = sw_device_open,
    .release = sw_device_release
};

#define SUCCESS 0
#define KEY_DEV_NAME "KEY"
#define SW_DEV_NAME "SW"
#define MAX_SIZE 256          // we assume that no message longer than this will be used

static struct miscdevice keydev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = KEY_DEV_NAME,
    .fops = &keydev_fops,
    .mode = 0666
};

static struct miscdevice swdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = SW_DEV_NAME,
    .fops = &swdev_fops,
    .mode = 0666
};

static char keydev_registered = 0;
static int swdev_registered = 0;
static char dev_msg[MAX_SIZE]; 
void *LW_virtual;
volatile int *KEY_ptr;
volatile int *SW_ptr;

static int __init start_chardev(void) {
    int err_key = misc_register (&keydev);
    int err_sw = misc_register (&swdev);
    if (err_key < 0 && err_sw < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", KEY_DEV_NAME);
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", SW_DEV_NAME);
        return -1;
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", KEY_DEV_NAME);
        printk (KERN_INFO "/dev/%s driver registered\n", SW_DEV_NAME);
        keydev_registered = 1;
        swdev_registered = 1;
    }

    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

    KEY_ptr = LW_virtual + KEY_BASE;
    SW_ptr = LW_virtual + SW_BASE;

    //strcpy (chardev_msg, "Hello from keyswdev\n"); /* initialize the message */
    return 0;
}

static void __exit stop_chardev(void) {
    if (keydev_registered && swdev_registered) {
        misc_deregister (&keydev);
        misc_deregister (&swdev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", KEY_DEV_NAME);
        printk (KERN_INFO "/dev/%s driver de-registered\n", SW_DEV_NAME);
    }
}

static int key_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static int key_device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t key_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    
    size_t bytes;

    if(*offset != 0){
        *offset = 0;
        return 0;
    }

    sprintf(dev_msg,"%x\n",*(KEY_ptr + 3));

    bytes = strlen (dev_msg);
    if(length < bytes){
        printk (KERN_ERR "Error: buffer's length too small\n");
        return -EMSGSIZE;
    }

    if (copy_to_user (buffer, dev_msg, bytes) != 0){
        printk (KERN_ERR "Error: copy_to_user unsuccessful\n");
        return -EFAULT;
    }

    *offset = length;
    return length;
}

static int sw_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static int sw_device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t sw_device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    
    size_t bytes;

    if(*offset != 0){
        *offset = 0;
        return 0;
    }

    sprintf(dev_msg,"%03x\n", SW_ptr);

    bytes = strlen (dev_msg);
    if(length < bytes){
        printk (KERN_ERR "Error: buffer's length too small\n");
        return -EMSGSIZE;
    }

    if (copy_to_user (buffer, dev_msg, bytes) != 0){
        printk (KERN_ERR "Error: copy_to_user unsuccessful\n");
        return -EFAULT;
    }

    *offset = bytes;
    return bytes;
}

MODULE_LICENSE("GPL");
module_init (start_chardev);
module_exit (stop_chardev);
