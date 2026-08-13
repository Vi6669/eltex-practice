#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

// Подсчет контрольной суммы пакета
unsigned short calculate_checksum(unsigned short *ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    unsigned short answer;

    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (unsigned short)~sum;

    return answer;
}

// Заполнение UDP-заголовка и расчет итоговой контрольной суммы
int build_udp_packet(char *packet_buf, uint32_t src_ip, uint32_t dest_ip, 
                     uint16_t src_port, uint16_t dest_port, 
                     const char *payload, int payload_len) {
    
    struct udphdr *udp = (struct udphdr *) packet_buf;
    char *data = packet_buf + sizeof(struct udphdr);
    
    // Копируем пользовательские данные вслед за UDP-заголовком
    memcpy(data, payload, payload_len);
    
    // Заполнение стандартных полей UDP заголовка
    udp->uh_sport = htons(src_port);
    udp->uh_dport = htons(dest_port);
    udp->uh_ulen = htons(sizeof(struct udphdr) + payload_len);
    udp->uh_sum = 0; // Временный сброс перед расчетом контрольной суммы
    
    // Сборка псевдозаголовка для вычисления checksum
    struct pseudo_header psh;
    psh.source_address = src_ip;
    psh.destination_address = dest_ip;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_UDP;
    psh.udp_length = htons(sizeof(struct udphdr) + payload_len);
    
    int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + payload_len;
    char *pseudogram = malloc(psize);
    if (!pseudogram) {
        perror("malloc failed");
        return -1;
    }
    
    memcpy(pseudogram, (char*) &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), udp, sizeof(struct udphdr) + payload_len);
    
    udp->uh_sum = calculate_checksum((unsigned short*) pseudogram, psize);
    
    free(pseudogram);
    return sizeof(struct udphdr) + payload_len;
}