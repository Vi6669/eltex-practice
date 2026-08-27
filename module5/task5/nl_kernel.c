#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/net_namespace.h>

#define NETLINK_USER 31

/* СООБЩЕНИЕ ЯДРА: Меняйте этот текст здесь при необходимости */
#define KERNEL_RESPONSE_MSG "Hello from kernel"

static struct sock *nl_sk = NULL;

static void hello_nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int msg_size;
    /* Переменная msg теперь использует вынесенный макрос */
    const char *msg = KERNEL_RESPONSE_MSG;
    int res;

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

    if (!skb) {
        printk(KERN_ERR "Received NULL skb\n");
        return;
    }

    if (skb->len < sizeof(struct nlmsghdr)) {
        printk(KERN_ERR "Received skb is too short\n");
        return;
    }

    msg_size = strlen(msg);

    nlh = (struct nlmsghdr *)skb->data;
    if (!nlh) {
        printk(KERN_ERR "Netlink header is NULL\n");
        return;
    }

    printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));
    pid = nlh->nlmsg_pid;

    skb_out = nlmsg_new(msg_size, GFP_ATOMIC);
    if (!skb_out)
    {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    if (!nlh)
    {
        printk(KERN_ERR "Failed to put nlmsg\n");
        nlmsg_free(skb_out);
        return;
    }

    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh), msg, msg_size);

    if (!nl_sk) {
        printk(KERN_ERR "Netlink socket is NULL, cannot send response\n");
        nlmsg_free(skb_out);
        return;
    }

    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0)
        printk(KERN_INFO "Error while sending back to user\n");
}

static struct netlink_kernel_cfg cfg = {
   .groups  = 1,
   .input = hello_nl_recv_msg,
};

static int __init hello_init(void)
{
    printk("Entering: %s\n", __FUNCTION__);
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);

    if (!nl_sk)
    {
        printk(KERN_ALERT "Error creating socket.\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "exiting hello module\n");
    if (nl_sk) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Царенкова Виктория");
MODULE_DESCRIPTION("Задание 4");