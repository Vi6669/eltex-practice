#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/init.h>

MODULE_LICENSE("67License v1.0");
MODULE_AUTHOR("Царенкова Виктория");
MODULE_DESCRIPTION("Попытка 1 написать модуль для лабы 1");

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello world! Оно реально работает? Я в шоке! Всем Привет!\n");
    return 0; 
}

static void __exit hello_cleanup(void)
{
    printk(KERN_INFO "Goodbye world! Модуль выгружен. Ура!\n");
}

module_init(hello_init);
module_exit(hello_cleanup);