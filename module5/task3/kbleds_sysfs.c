#include <linux/module.h>
#include <linux/configfs.h>
#include <linux/init.h>
#include <linux/tty.h>          
#include <linux/kd.h>           
#include <linux/vt.h>
#include <linux/console_struct.h>       
#include <linux/vt_kern.h>
#include <linux/timer.h>
#include <linux/printk.h> 
#include <linux/kobject.h> 
#include <linux/sysfs.h> 
#include <linux/fs.h> 
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Царенкова Виктория");
MODULE_DESCRIPTION("Задание 3");


static struct timer_list my_timer;
static struct tty_driver *my_driver;
static int _kbledstatus = 0;

#define BLINK_DELAY   (HZ/5)
#define RESTORE_LEDS  0xFF


static int test = 3; 

static struct kobject *example_kobject;

static void my_timer_func(struct timer_list *ptr)
{
        int *pstatus = &_kbledstatus;
        int current_mask = test; 

        if (*pstatus == current_mask) {
                *pstatus = RESTORE_LEDS;
        } else {
                *pstatus = current_mask;
        }

        if (my_driver && my_driver->ops && my_driver->ops->ioctl && 
            vc_cons[fg_console].d && vc_cons[fg_console].d->port.tty) {
                
                (my_driver->ops->ioctl)(vc_cons[fg_console].d->port.tty, KDSETLED, *pstatus);
        }

        my_timer.expires = jiffies + BLINK_DELAY;
        add_timer(&my_timer);
}


static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        pr_info("kbleds_sysfs: sysfs read test = %d\n", test);
        return sprintf(buf, "%d\n", test);
}
 

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
        int temp_val;
        int res;

        
        res = kstrtoint(buf, 10, &temp_val);
        if (res < 0) {
                pr_err("kbleds_sysfs: validation failed. Input contains invalid characters or is not a number!\n");
                return -EINVAL; 
        }

        if (temp_val < 0 || temp_val > 7) {
                pr_err("kbleds_sysfs: validation failed. Value %d is out of range (must be 0 to 7)!\n", temp_val);
                return -EINVAL; 
        }

        
        test = temp_val;
        pr_info("kbleds_sysfs: successfully validated and updated test value to %d\n", test);

        return count;
}

static struct kobj_attribute foo_attribute = __ATTR(test, 0660, foo_show, foo_store);


static int __init kbleds_sysfs_init(void)
{
        int i;
        int error = 0;
 
        pr_info("kbleds_sysfs: loading module\n");
        pr_info("kbleds_sysfs: fgconsole is %x\n", fg_console);
        
        for (i = 0; i < MAX_NR_CONSOLES; i++) {
                if (!vc_cons[i].d)
                        break;
                pr_info("kbleds_sysfs: console[%i/%i] #%i, tty %lx\n", i,
                       MAX_NR_CONSOLES, vc_cons[i].d->vc_num,
                       (unsigned long)vc_cons[i].d->port.tty);
        }

        if (!vc_cons[fg_console].d || !vc_cons[fg_console].d->port.tty) {
                pr_err("kbleds_sysfs: Error - active tty console not found\n");
                return -ENODEV;
        }

        my_driver = vc_cons[fg_console].d->port.tty->driver;
        if (!my_driver) {
                pr_err("kbleds_sysfs: Error - tty driver is NULL\n");
                return -ENODEV;
        }
      
        pr_info("kbleds_sysfs: tty driver major %d\n", my_driver->major);
        
        timer_setup(&my_timer, my_timer_func, 0);
        my_timer.expires = jiffies + BLINK_DELAY;
        add_timer(&my_timer);

        example_kobject = kobject_create_and_add("systest", kernel_kobj);
        if (!example_kobject) {
                pr_err("kbleds_sysfs: Failed to create kobject\n");
                del_timer(&my_timer);
                return -ENOMEM;
        }
 
        error = sysfs_create_file(example_kobject, &foo_attribute.attr);
        if (error) {
                pr_err("kbleds_sysfs: failed to create the test file in /sys/kernel/systest\n");
                kobject_put(example_kobject);
                del_timer(&my_timer);
                return error;
        }
 
        pr_info("kbleds_sysfs: Module initialized successfully\n");
        return 0;
}


static void __exit kbleds_sysfs_exit(void)
{
        pr_info("kbleds_sysfs: unloading module\n");
        
        del_timer(&my_timer);
        
        if (my_driver && my_driver->ops && my_driver->ops->ioctl && 
            vc_cons[fg_console].d && vc_cons[fg_console].d->port.tty) {
                (my_driver->ops->ioctl)(vc_cons[fg_console].d->port.tty, KDSETLED, RESTORE_LEDS);
        }

        if (example_kobject) {
                sysfs_remove_file(example_kobject, &foo_attribute.attr);
                kobject_put(example_kobject);
        }
        
        pr_info("kbleds_sysfs: Module unloaded successfully\n");
}

module_init(kbleds_sysfs_init);
module_exit(kbleds_sysfs_exit);
