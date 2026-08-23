#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define PROC_NAME "hello_proc"  
#define MSG_SIZE 128        

MODULE_LICENSE("GPL");      
MODULE_AUTHOR("Царенкова Виктория");
MODULE_DESCRIPTION("Задание 2, дай бог чтобы работало");

static int len;
static int temp;
static char *msg = NULL;

static ssize_t read_proc(struct file *filp, char __user *buf, size_t count, loff_t *offp) {
    if (count > temp) {
        count = temp;
    }
    temp = temp - count;
    
    if (copy_to_user(buf, msg, count)) {
        return -EFAULT;
    }
    
    if (count == 0)
        temp = len;
    return count;
}
 
static ssize_t write_proc(struct file *filp, const char __user *buf, size_t count, loff_t *offp) {
    size_t to_copy = (count >= MSG_SIZE) ? (MSG_SIZE - 1) : count;
    
    if (copy_from_user(msg, buf, to_copy)) {
        return -EFAULT;
    }
    
    msg[to_copy] = '\0';
    
    len = to_copy;
    temp = len;
    return count;
}
 
static const struct proc_ops proc_fops = {
    proc_read: read_proc,
    proc_write: write_proc,
};
 
static void create_new_proc_entry(void) { 
    proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    msg = kmalloc(MSG_SIZE, GFP_KERNEL);
}
 
static int proc_init(void) {
    create_new_proc_entry();
    return 0;
}
 
static void proc_cleanup(void) {
    remove_proc_entry(PROC_NAME, NULL);
    kfree(msg);
}
 
module_init(proc_init);
module_exit(proc_cleanup);