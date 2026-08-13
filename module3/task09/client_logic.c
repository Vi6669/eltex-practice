#include "client_logic.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

int reserve_local_port(uint32_t local_ip_bin, uint16_t client_port) {
    int dummy_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dummy_fd >= 0) {
        struct sockaddr_in dummy_addr;
        memset(&dummy_addr, 0, sizeof(dummy_addr));
        dummy_addr.sin_family = AF_INET;
        dummy_addr.sin_port = htons(client_port);
        dummy_addr.sin_addr.s_addr = local_ip_bin;
        if (bind(dummy_fd, (struct sockaddr *)&dummy_addr, sizeof(dummy_addr)) < 0) {
            perror("Warning: failed to bind dummy socket to reserve port");
        }
    }
    return dummy_fd;
}

void send_raw_message(int sockfd, uint32_t local_ip, uint32_t dest_ip, 
                      uint16_t src_port, uint16_t dest_port, const char *message) {
    char send_buf[BUFFER_SIZE];
    int packet_len = build_udp_packet(send_buf, local_ip, dest_ip, src_port, dest_port, message, strlen(message));
    if (packet_len < 0) return;

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = dest_ip;

    if (sendto(sockfd, send_buf, packet_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Sendto failed");
    }
}

int wait_for_server_reply(int sockfd, uint16_t client_port, uint16_t server_port) {
    char recv_buf[BUFFER_SIZE];
    int retries = 5; // Пробуем вычитать входящие пакеты несколько раз в случае сильного шума в сети

    while (retries > 0) {
        int r_len = recvfrom(sockfd, recv_buf, BUFFER_SIZE, 0, NULL, NULL);
        if (r_len < 0) {
            retries--;
            continue;
        }

        struct iphdr *ip = (struct iphdr *)recv_buf;
        int ip_hdr_len = ip->ihl * 4;

        if (ip->protocol == IPPROTO_UDP) {
            struct udphdr *udp = (struct udphdr *)(recv_buf + ip_hdr_len);
            uint16_t d_port = ntohs(udp->uh_dport);
            uint16_t s_port = ntohs(udp->uh_sport);

            // Фильтруем: пакет должен быть адресован нашему клиенту и отправлен с порта сервера
            if (d_port == client_port && s_port == server_port) {
                int u_len = ntohs(udp->uh_ulen);
                int p_len = u_len - sizeof(struct udphdr);
                char *p_data = recv_buf + ip_hdr_len + sizeof(struct udphdr);

                char response[BUFFER_SIZE];
                memcpy(response, p_data, p_len);
                response[p_len] = '\0';

                printf("[CLIENT] Reply from server: \"%s\"\n", response);
                return 1; // Успешно получили ответ
            }
        }
        retries--;
    }
    return 0; // Таймаут
}

void send_clean_shutdown(int sockfd, uint32_t local_ip, uint32_t dest_ip, 
                         uint16_t src_port, uint16_t dest_port) {
    printf("\n[CLIENT] Exiting cleanly, notifying server...\n");
    send_raw_message(sockfd, local_ip, dest_ip, src_port, dest_port, SHUTDOWN_MSG);
}