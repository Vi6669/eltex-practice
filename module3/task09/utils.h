#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <sys/types.h>

#define BUFFER_SIZE 65536
// Служебное сообщение, сообщающее серверу об отключении клиента
#define SHUTDOWN_MSG "CLIENT_SHUTDOWN_SIGNAL"

struct pseudo_header {
    uint32_t source_address;      // IP-адрес отправителя
    uint32_t destination_address; // IP-адрес получателя
    uint8_t placeholder;          // Всегда равен 0 
    uint8_t protocol;             // Код протокола 
    uint16_t udp_length;          // Длина UDP-заголовка + полезная нагрузка
};

unsigned short calculate_checksum(unsigned short *ptr, int nbytes);

int build_udp_packet(char *packet_buf, uint32_t src_ip, uint32_t dest_ip, 
                     uint16_t src_port, uint16_t dest_port, 
                     const char *payload, int payload_len);

#endif // UTILS_H