#include "packet_processor.h"
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include "sniffer.h"

bool parse_udp_packet(const unsigned char *buffer, int packet_size, ParsedPacket *pkt) {
    if (packet_size < 14) return false;

    int ip_offset = 0;
    // Извлекаем тип протокола из Ethernet-заголовка
    uint16_t eth_proto = ntohs(*(uint16_t*)(buffer + 12));

    // 1. Стандартный Ethernet (IPv4)
    if (eth_proto == ETH_P_IP) {
        ip_offset = 14;
        memcpy(pkt->src_mac, buffer + 6, 6);
        memcpy(pkt->dest_mac, buffer + 0, 6);
    } 
    // 2. Ethernet с тегом VLAN (802.1Q) - Частая ситуация в виртуальных машинах!
    else if (eth_proto == 0x8100) {
        if (packet_size < 18) return false;
        // Проверяем, что внутри VLAN-тега лежит IPv4
        if (ntohs(*(uint16_t*)(buffer + 16)) != ETH_P_IP) return false; 
        ip_offset = 18; // Смещаемся на 4 байта дальше из-за VLAN
        memcpy(pkt->src_mac, buffer + 6, 6);
        memcpy(pkt->dest_mac, buffer + 0, 6);
    } 
    // 3. Интерфейс локальной петли (lo) - вообще нет Ethernet заголовка
    else if ((buffer[0] >> 4) == 4) {
        ip_offset = 0;
        memset(pkt->src_mac, 0, 6);
        memset(pkt->dest_mac, 0, 6);
    } 
    else {
        return false; // Игнорируем ARP, IPv6 и прочее
    }

    // ПАРСИНГ L3 (Сетевой уровень - IP)
    if (packet_size < ip_offset + (int)sizeof(struct iphdr)) return false;
    const struct iphdr *ip = (const struct iphdr *)(buffer + ip_offset);

    if (ip->protocol != IPPROTO_UDP) return false; // Пропускаем TCP, ICMP и др.

    // ПАРСИНГ L4 (Транспортный уровень - UDP)
    int ip_header_len = ip->ihl * 4;
    int udp_offset = ip_offset + ip_header_len;
    
    if (packet_size < udp_offset + (int)sizeof(struct udphdr)) return false;
    const struct udphdr *udp = (const struct udphdr *)(buffer + udp_offset);

    pkt->src_ip.s_addr = ip->saddr;
    pkt->dest_ip.s_addr = ip->daddr;
    pkt->src_port = ntohs(udp->source);
    pkt->dest_port = ntohs(udp->dest);

    // Извлечение полезной нагрузки
    int payload_offset = udp_offset + sizeof(struct udphdr);
    pkt->payload_len = ntohs(udp->len) - sizeof(struct udphdr);

    if (pkt->payload_len > 0 && packet_size >= payload_offset + pkt->payload_len) {
        pkt->payload = buffer + payload_offset;
    } else {
        pkt->payload = NULL;
        pkt->payload_len = 0;
    }

    return true;
}

void print_payload(const unsigned char *payload, int len, FILE *log_file) {
    fprintf(stdout, "Payload (%d bytes):\n  ASCII: ", len);
    if (log_file) fprintf(log_file, "Payload (%d bytes):\n  ASCII: ", len);
    
    for (int i = 0; i < len; i++) {
        char c = payload[i];
        bool is_printable = (c >= 32 && c <= 126);
        bool is_whitespace = (c == '\n' || c == '\r' || c == '\t');
        
        char out_char = is_printable ? c : (is_whitespace ? ' ' : '.');
        fprintf(stdout, "%c", out_char);
        if (log_file) fprintf(log_file, "%c", out_char);
    }
    
    fprintf(stdout, "\n  HEX:   ");
    if (log_file) fprintf(log_file, "\n  HEX:   ");
    
    for (int i = 0; i < len; i++) {
        fprintf(stdout, "%02x ", payload[i]);
        if (log_file) fprintf(log_file, "%02x ", payload[i]);
        
        if ((i + 1) % 16 == 0 && i < len - 1) {
            fprintf(stdout, "\n         ");
            if (log_file) fprintf(log_file, "\n         ");
        }
    }
    fprintf(stdout, "\n");
    if (log_file) fprintf(log_file, "\n");
}

void process_and_log_packet(const ParsedPacket *pkt, double elapsed, int choice, FILE *log_file) {
    char src_mac_str[20], dest_mac_str[20];
    format_mac(pkt->src_mac, src_mac_str);
    format_mac(pkt->dest_mac, dest_mac_str);

    char src_ip_str[16], dest_ip_str[16];
    strcpy(src_ip_str, inet_ntoa(pkt->src_ip));
    strcpy(dest_ip_str, inet_ntoa(pkt->dest_ip));

    #define PRINT_BOTH(fmt, ...) \
        do { \
            fprintf(stdout, fmt, ##__VA_ARGS__); \
            if (log_file) fprintf(log_file, fmt, ##__VA_ARGS__); \
        } while(0)

    PRINT_BOTH("\n[Время с начала захвата: +%.4f сек]\n", elapsed);
    PRINT_BOTH("  MAC:  %s -> %s\n", src_mac_str, dest_mac_str);
    PRINT_BOTH("  IP:   %s -> %s\n", src_ip_str, dest_ip_str);
    PRINT_BOTH("  UDP:  Порт %d -> Порт %d\n", pkt->src_port, pkt->dest_port);
    
    if (choice == 1 && pkt->payload_len == (int)sizeof(ChatPacket)) {
        const ChatPacket *chat = (const ChatPacket *)pkt->payload;
        
        PRINT_BOTH("  [Перехвачено событие чата (Задание 6)]\n");
        PRINT_BOTH("    PID процесса: %d\n", chat->pid);
        PRINT_BOTH("    Никнейм:      %s\n", chat->nickname);
        
        const char *type_str = "UNKNOWN";
        if (chat->type == MSG_JOIN) type_str = "MSG_JOIN (Вход)";
        else if (chat->type == MSG_CHAT) type_str = "MSG_CHAT (Текст)";
        else if (chat->type == MSG_LEAVE) type_str = "MSG_LEAVE (Выход)";
        
        PRINT_BOTH("    Тип события:  %s\n", type_str);
        if (chat->type == MSG_CHAT) {
            PRINT_BOTH("    Сообщение:    \"%s\"\n", chat->text);
        }
    } else {
        if (pkt->payload_len > 0 && pkt->payload != NULL) {
            print_payload(pkt->payload, pkt->payload_len, log_file);
        } else {
            PRINT_BOTH("  Payload: Пустой UDP-сегмент\n");
        }
    }
    PRINT_BOTH("--------------------------------------------------------\n");
    
    fflush(stdout);
    if (log_file) fflush(log_file);
}