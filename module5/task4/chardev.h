#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/fs.h>

#define SUCCESS 0 
#define DEVICE_NAME "chardev" 
#define BUF_LEN 80            


static int device_open(struct inode *, struct file *); 
static int device_release(struct inode *, struct file *); 
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *); 
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *); 

#endif /* CHARDEV_H */