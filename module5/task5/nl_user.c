#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024


#define USER_SEND_MSG "Hello"

static struct sockaddr_nl src_addr, dest_addr;
static struct nlmsghdr *nlh = NULL;
static struct iovec iov;
static int sock_fd = -1;
static struct msghdr msg;

int main()
{
    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("Bind failed");
        close(sock_fd);
        return -1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0;
    dest_addr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
    if (!nlh) {
        perror("Memory allocation failed");
        close(sock_fd);
        return -1;
    }
    
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;


    strncpy(NLMSG_DATA(nlh), USER_SEND_MSG, MAX_PAYLOAD - 1);

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    printf("Sending message to kernel\n");
    
    if (sendmsg(sock_fd, &msg, 0) < 0) {
        perror("Sendmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }
    
    printf("Waiting for message from kernel\n");

    if (recvmsg(sock_fd, &msg, 0) < 0) {
        perror("Recvmsg failed");
        free(nlh);
        close(sock_fd);
        return -1;
    }
    
    printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh));
    
    free(nlh);
    close(sock_fd);
    return 0;
}