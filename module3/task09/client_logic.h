#ifndef CLIENT_LOGIC_H
#define CLIENT_LOGIC_H

#include <stdint.h>
#include "utils.h"

/* 
 * Создает фиктивный стандартный сокет UDP и биндит его на локальный порт.
 * Это нужно, чтобы ядро ОС не отправляло серверу автоматический ICMP-ответ "Port Unreachable".
 */
int reserve_local_port(uint32_t local_ip_bin, uint16_t client_port);

// Ручная сборка и отправка UDP пакета через сырой сокет
void send_raw_message(int sockfd, uint32_t local_ip, uint32_t dest_ip, 
                      uint16_t src_port, uint16_t dest_port, const char *message);

// Ожидание ответа от сервера и фильтрация пакетов по связке портов
int wait_for_server_reply(int sockfd, uint16_t client_port, uint16_t server_port);

// Отправка специального пакета уведомления сервера о штатном отключении клиента
void send_clean_shutdown(int sockfd, uint32_t local_ip, uint32_t dest_ip, 
                         uint16_t src_port, uint16_t dest_port);

#endif // CLIENT_LOGIC_H