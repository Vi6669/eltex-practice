#ifndef SERVER_LOGIC_H
#define SERVER_LOGIC_H

#include <stdint.h>
#include "utils.h"

#define MAX_CLIENTS 128

/*
 * Структура записи о клиенте (САОД).
 * Позволяет серверу отслеживать сообщения от каждого уникального клиента.
 */
struct ClientRecord {
    uint32_t ip;       // Бинарный IP-адрес клиента (сетевой порядок байт)
    uint16_t port;     // Порт клиента
    int message_count; // Счетчик сообщений, полученных от этого клиента
};

// Инициализация базы данных клиентов
void init_client_db(void);

// Поиск или регистрация нового клиента в базе данных (возвращает индекс в массиве)
int find_or_create_client(uint32_t ip, uint16_t port);

// Сброс счетчика сообщений клиента (вызывается при получении служебного пакета)
void reset_client(uint32_t ip, uint16_t port);

// Обработка одного принятого RAW пакета, валидация UDP и отправка эхо-ответа
void process_incoming_packet(int sockfd, char *buffer, int packet_len, uint16_t server_port);

#endif // SERVER_LOGIC_H