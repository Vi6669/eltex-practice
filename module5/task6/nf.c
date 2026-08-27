#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/netfilter_ipv4.h>
#include<linux/skbuff.h>
#include<linux/ip.h>
#include<linux/inet.h>

#include<linux/proc_fs.h>
#include<linux/fs.h>
#include<linux/slab.h>
#include<linux/list.h>
#include<linux/spinlock.h>
#include<linux/version.h>
#include<linux/uaccess.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Царенкова Виктория");
MODULE_DESCRIPTION("Задание 6");


struct ip_node {
    __be32 ip;
    struct list_head list;
};

static LIST_HEAD(blacklist_ips);
static DEFINE_SPINLOCK(blacklist_lock);
static struct proc_dir_entry *proc_entry;
 
static struct nf_hook_ops nfin;
 
static unsigned int hook_func_in(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct ethhdr *eth;
    struct iphdr *ip_header;
    struct ip_node *entry;
    unsigned int action = NF_ACCEPT;
 
    if (!skb)
        return NF_ACCEPT;

    eth = (struct ethhdr*)skb_mac_header(skb);
    ip_header = (struct iphdr *)skb_network_header(skb);

    if (!ip_header)
        return NF_ACCEPT;

   
    if (eth && skb_mac_header_was_set(skb)) {
        printk(KERN_INFO "src mac %pM, dst mac %pM\n", eth->h_source, eth->h_dest);
    }
    
    printk(KERN_INFO "src IP addr: %pI4\n", &ip_header->saddr);

    spin_lock(&blacklist_lock);
    list_for_each_entry(entry, &blacklist_ips, list) {
        if (entry->ip == ip_header->daddr) {
            action = NF_DROP;
            break;
        }
    }
    spin_unlock(&blacklist_lock);

    if (action == NF_DROP) {
        printk(KERN_INFO "Blocked outgoing packet to: %pI4\n", &ip_header->daddr);
    }

    return action;
}


static ssize_t blacklist_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char *kbuf;
    size_t len = 0;
    struct ip_node *entry;
    ssize_t ret;

    kbuf = kmalloc(4096, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    spin_lock(&blacklist_lock);
    list_for_each_entry(entry, &blacklist_ips, list) {
        len += scnprintf(kbuf + len, 4096 - len, "%pI4\n", &entry->ip);
        if (len >= 4096 - 32)
            break;
    }
    spin_unlock(&blacklist_lock);

    ret = simple_read_from_buffer(buf, count, ppos, kbuf, len);
    kfree(kbuf);
    return ret;
}


static ssize_t blacklist_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char kbuf[64];
    size_t len;
    char action;
    char ip_str[32];
    __be32 ip;
    struct ip_node *entry, *tmp;
    int found = 0;

    len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    kbuf[len] = '\0';


    while (len > 0 && (kbuf[len - 1] == '\n' || kbuf[len - 1] == '\r' || kbuf[len - 1] == ' ')) {
        kbuf[len - 1] = '\0';
        len--;
    }

    if (len < 2)
        return -EINVAL;

    action = kbuf[0];
    strscpy(ip_str, kbuf + 1, sizeof(ip_str));


    if (in4_pton(ip_str, -1, (u8 *)&ip, -1, NULL) == 0) {
        printk(KERN_ERR "Invalid IP address format: %s\n", ip_str);
        return -EINVAL;
    }

    if (action == '+') {
        struct ip_node *new_node = kmalloc(sizeof(*new_node), GFP_KERNEL);
        if (!new_node)
            return -ENOMEM;
        new_node->ip = ip;
        INIT_LIST_HEAD(&new_node->list);

        spin_lock(&blacklist_lock);
        list_for_each_entry(entry, &blacklist_ips, list) {
            if (entry->ip == ip) {
                found = 1;
                break;
            }
        }
        if (!found) {
            list_add_tail(&new_node->list, &blacklist_ips);
            printk(KERN_INFO "Added %pI4 to blacklist\n", &ip);
        } else {
            kfree(new_node);
        }
        spin_unlock(&blacklist_lock);

    } else if (action == '-') {
        spin_lock(&blacklist_lock);
        list_for_each_entry_safe(entry, tmp, &blacklist_ips, list) {
            if (entry->ip == ip) {
                list_del(&entry->list);
                kfree(entry);
                printk(KERN_INFO "Removed %pI4 from blacklist\n", &ip);
                break;
            }
        }
        spin_unlock(&blacklist_lock);
    } else {
        return -EINVAL;
    }

    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_fops = {
    .proc_read = blacklist_read,
    .proc_write = blacklist_write,
};
#else
static const struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .read = blacklist_read,
    .write = blacklist_write,
};
#endif

static int __init init_main(void)
{
    int ret;

    proc_entry = proc_create("ip_blacklist", 0666, NULL, &proc_fops);
    if (!proc_entry) {
        printk(KERN_ERR "Failed to create /proc/ip_blacklist\n");
        return -ENOMEM;
    }

    nfin.hook     = hook_func_in;
    nfin.hooknum  = NF_INET_LOCAL_OUT; 
    nfin.pf       = PF_INET;
    nfin.priority = NF_IP_PRI_FIRST;

    ret = nf_register_net_hook(&init_net, &nfin); 
    if (ret) {
        printk(KERN_ERR "Failed to register netfilter hook\n");
        proc_remove(proc_entry);
        return ret;
    }
    
    return 0;
}
 
static void __exit cleanup_main(void)
{
    struct ip_node *entry, *tmp;

    nf_unregister_net_hook(&init_net, &nfin); 
    
    if (proc_entry) {
        proc_remove(proc_entry);
    }

   
    spin_lock(&blacklist_lock);
    list_for_each_entry_safe(entry, tmp, &blacklist_ips, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    spin_unlock(&blacklist_lock);
}

module_init(init_main);
module_exit(cleanup_main);