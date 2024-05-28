#include <linux/fs.h>           // struct file, struct file_operations
#include <linux/init.h>         // for __init, see code
#include <linux/module.h>       // for module init and exit macros
#include <linux/miscdevice.h>   // for misc_device_register and struct miscdev
#include <linux/uaccess.h>      // for copy_to_user, see code
#include <asm/io.h>             // for mmap
#include "../../inc/address_map_arm.h"
#include "../../inc/interrupt_ID.h"

/* Kernel character device driver. By default, this driver provides the text "Hello from
 * chardev" when /dev/chardev is read (for example, cat /dev/chardev). The text can be changed
 * by writing a new string to /dev/chardev (for example echo "New message" > /dev/chardev).
 * This version of the code uses copy_to_user and copy_from_user, to send data to, and receive
 * date from, user-space */

static int ledr_device_open (struct inode *, struct file *);
static int ledr_device_release (struct inode *, struct file *);
static ssize_t ledr_device_write(struct file *, const char *, size_t, loff_t *);

static int hex_device_open (struct inode *, struct file *);
static int hex_device_release (struct inode *, struct file *);
static ssize_t hex_device_write(struct file *, const char *, size_t, loff_t *);
void segmentDisplay(int number);

static struct file_operations ledrdev_fops = {
    .owner = THIS_MODULE,
    .write = ledr_device_write,
    .open = ledr_device_open,
    .release = ledr_device_release
};

static struct file_operations hexdev_fops = {
    .owner = THIS_MODULE,
    .write = hex_device_write,
    .open = hex_device_open,
    .release = hex_device_release
};

#define SUCCESS 0
#define LEDR_DEV_NAME "LEDR"
#define HEX_DEV_NAME "HEX"
#define MAX_SIZE 256        // we assume that no message longer than this will be used

static struct miscdevice ledrdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = LEDR_DEV_NAME,
    .fops = &ledrdev_fops,
    .mode = 0666
};

static struct miscdevice hexdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = HEX_DEV_NAME,
    .fops = &hexdev_fops,
    .mode = 0666
};

static int ledrdev_registered = 0;
static int hexdev_registered = 0;
static int ledrdev_msg;  
static int hexdev_msg;
void *LW_virtual;
volatile int *HEX03_ptr;
volatile int *HEX45_ptr;
volatile int *LEDR_ptr;

const char hex_code[] = {
    0b00111111, //0
    0b00000110, //1
    0b01011001, //2
    0b01001111, //3
    0b01100110, //4
    0b01101101, //5
    0b01111101, //6
    0b00000111, //7
    0b01111111, //8
    0b01101111, //9
};

static int __init start_chardev(void) {
    int err_ledr = misc_register (&ledrdev);
    int err_hex = misc_register (&hexdev);
    if (err_ledr < 0 && err_hex < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", LEDR_DEV_NAME);
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", HEX_DEV_NAME);
        return -1;
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", LEDR_DEV_NAME);
        printk (KERN_INFO "/dev/%s driver registered\n", HEX_DEV_NAME);
        chardev_registered = 1;
    }

    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    HEX03_ptr = LW_virtual + HEX3_HEX0_BASE;
    HEX45_ptr = LW_virtual + HEX5_HEX4_BASE;
    *HEX03_ptr = hex_code[0];
    *HEX45_ptr = hex_code[0];

    LEDR_ptr = LW_virtual + LEDR_BASE;

    return 0;
}

static void __exit stop_chardev(void) {
    if (ledrdev_registered && hexdev_registered) {
        misc_deregister (&ledrdev);
        misc_deregister (&hexdev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", LEDR_DEV_NAME);
        printk (KERN_INFO "/dev/%s driver de-registered\n", HEX_DEV_NAME);
    }
}

/* Called when a process opens chardev */
static int ledr_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

/* Called when a process closes chardev */
static int ledr_device_release(struct inode *inode, struct file *file) {
    return 0;
}

/* Called when a process writes to chardev. Stores the data received into chardev_msg, and
 * returns the number of bytes stored. */
static ssize_t ledr_device_write(struct file *filp, const char *buffer, 
    size_t length, loff_t *offset) {

    size_t bytes;
    bytes = length;

    // if message is longer than expected, it's truncated
    // last byte is saved for NULL
    if (bytes > MAX_SIZE - 1){
        bytes = MAX_SIZE - 1;
    }

    sscanf(buffer, "%x\n", &ledrdev_msg);
    *LEDR_ptr = ledrdev_msg;

    return bytes;
}

/* Called when a process opens chardev */
static int hex_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

/* Called when a process closes chardev */
static int hex_device_release(struct inode *inode, struct file *file) {
    return 0;
}

/* Called when a process writes to chardev. Stores the data received into chardev_msg, and
 * returns the number of bytes stored. */
static ssize_t hex_device_write(struct file *filp, const char *buffer, 
    size_t length, loff_t *offset) {

    size_t bytes;
    bytes = length;

    // if message is longer than expected, it's truncated
    // last byte is saved for NULL
    if (bytes > MAX_SIZE - 1){
        bytes = MAX_SIZE - 1;
    }

    sscanf(buffer, "%x\n", &hexdev_msg);
    segmentDisplay(hexdev_msg);
    
    return bytes;
}

void segmentDisplay(int number) {
    int hex0, hex1, hex2, hex3, hex4, hex5;

    hex0 = number & 0xF;
    hex1 = (number >> 4) & 0xF;
    hex2 = (number >> 8) & 0xF;
    hex3 = (number >> 12) & 0xF;
    hex4 = (number >> 16) & 0xF;
    hex5 = (number >> 20) & 0xF;

    *HEX03_ptr = (hex_code[hex3] << 24) | (hex_code[hex2] << 16) | (hex_code[hex1] << 8) | hex_code[hex0];
    *HEX45_ptr = (hex_code[hex5] << 8) | hex_code[hex4];
}

MODULE_LICENSE("GPL");
module_init (start_chardev);
module_exit (stop_chardev);