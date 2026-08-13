#include "sniffer.h"
#include <sys/socket.h>
#include <features.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>  // Для htons и преобразования IP-адресов
#include <netinet/in.h> // Для констант протоколов

int init_raw_socket(const char *device) {
    // Создаем пакетный сокет для захвата всех IPv4 пакетов
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sock < 0) {
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_nametoindex(device);
    sll.sll_protocol = htons(ETH_P_IP);

    if (sll.sll_ifindex == 0) {
        close(sock);
        return -2; // Интерфейс с таким именем не найден
    }

    // Привязываем сокет к выбранному сетевому интерфейсу
    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        close(sock);
        return -3; // Ошибка привязки
    }

    return sock;
}

int set_promisc_mode(int sock, const char *device, bool enable) {
    struct packet_mreq mr;
    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = if_nametoindex(device);
    if (mr.mr_ifindex == 0) {
        return -1;
    }
    mr.mr_type = PACKET_MR_PROMISC;

    int opt = enable ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP;
    if (setsockopt(sock, SOL_PACKET, opt, &mr, sizeof(mr)) < 0) {
        return -2; // Ошибка переключения режима
    }
    return 0;
}

double get_elapsed_time(struct timespec start_time) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double start = start_time.tv_sec + (start_time.tv_nsec / 1000000000.0);
    double current = current_time.tv_sec + (current_time.tv_nsec / 1000000000.0);
    return current - start;
}

void format_mac(const unsigned char *mac, char *out_str) {
    sprintf(out_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}