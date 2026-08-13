#include "server_logic.h"
#include <stdio.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

// Локальная база данных клиентов (САОД)
static struct ClientRecord client_db[MAX_CLIENTS];
static int client_count = 0;

void init_client_db(void) {
    memset(client_db, 0, sizeof(client_db));
    client_count = 0;
}

int find_or_create_client(uint32_t ip, uint16_t port) {
    for (int i = 0; i < client_count; i++) {
        if (client_db[i].ip == ip && client_db[i].port == port) {
            return i;
        }
    }
    if (client_count < MAX_CLIENTS) {
        client_db[client_count].ip = ip;
        client_db[client_count].port = port;
        client_db[client_count].message_count = 0;
        client_count++;
        return client_count - 1;
    }
    return -1;
}

void reset_client(uint32_t ip, uint16_t port) {
    for (int i = 0; i < client_count; i++) {
        if (client_db[i].ip == ip && client_db[i].port == port) {
            client_db[i].message_count = 0;
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr addr = { .s_addr = ip };
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            printf("[SERVER] Reset counter for client %s:%d\n", ip_str, port);
            return;
        }
    }
}

// Вспомогательная внутренняя функция для отправки ответа через RAW сокет
static void send_raw_reply(int sockfd, uint32_t src_ip, uint32_t dest_ip, 
                           uint16_t src_port, uint16_t dest_port, 
                           const char *payload_str, int current_count) {
    char response_payload[BUFFER_SIZE];
    // Формируем эхо-ответ: оригинальный текст + счетчик сообщений
    int resp_len = snprintf(response_payload, sizeof(response_payload), "%s %d", payload_str, current_count);

    char send_buf[BUFFER_SIZE];
    int packet_to_send_len = build_udp_packet(send_buf, src_ip, dest_ip, src_port, dest_port, response_payload, resp_len);
    if (packet_to_send_len < 0) return;

    struct sockaddr_in daddr;
    memset(&daddr, 0, sizeof(daddr));
    daddr.sin_family = AF_INET;
    daddr.sin_addr.s_addr = dest_ip;

    if (sendto(sockfd, send_buf, packet_to_send_len, 0, (struct sockaddr *)&daddr, sizeof(daddr)) < 0) {
        perror("Sendto failed");
    } else {
        char ip_str[INET_ADDRSTRLEN];
        struct in_addr addr = { .s_addr = dest_ip };
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        printf("[SERVER] Sent reply to %s:%d: \"%s\"\n", ip_str, dest_port, response_payload);
    }
}

// Парсинг заголовков пакета и вызов соответствующей логики
void process_incoming_packet(int sockfd, char *buffer, int packet_len, uint16_t server_port) {
    (void)packet_len;

    struct iphdr *ip = (struct iphdr *)buffer;
    int ip_hdr_len = ip->ihl * 4;

    if (ip->protocol != IPPROTO_UDP) {
        return;
    }

    struct udphdr *udp = (struct udphdr *)(buffer + ip_hdr_len);
    uint16_t dest_port = ntohs(udp->uh_dport);
    uint16_t src_port = ntohs(udp->uh_sport);

    // Обрабатываем только те пакеты, которые пришли на порт сервера
    if (dest_port != server_port) {
        return;
    }

    int udp_len = ntohs(udp->uh_ulen);
    int payload_len = udp_len - sizeof(struct udphdr);
    char *payload = buffer + ip_hdr_len + sizeof(struct udphdr);

    char payload_str[BUFFER_SIZE];
    memcpy(payload_str, payload, payload_len);
    payload_str[payload_len] = '\0';

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->saddr), ip_str, sizeof(ip_str));

    // Проверяем, не является ли сообщение сигналом выключения
    if (strcmp(payload_str, SHUTDOWN_MSG) == 0) {
        reset_client(ip->saddr, src_port);
        return;
    }

    printf("[SERVER] Received from %s:%d: \"%s\"\n", ip_str, src_port, payload_str);

    int client_idx = find_or_create_client(ip->saddr, src_port);
    int current_count = 0;
    if (client_idx != -1) {
        client_db[client_idx].message_count++;
        current_count = client_db[client_idx].message_count;
    }

    // Отправляем эхо-ответ обратно клиенту
    send_raw_reply(sockfd, ip->daddr, ip->saddr, server_port, src_port, payload_str, current_count);
}