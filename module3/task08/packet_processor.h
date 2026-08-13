#ifndef PACKET_PROCESSOR_H
#define PACKET_PROCESSOR_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#define MAX_NICK_LEN 32
#define MAX_TEXT_LEN 512

// Типы сообщений из вашего Задания 6
typedef enum {
    MSG_JOIN = 1,
    MSG_CHAT = 2,
    MSG_LEAVE = 3
} MsgType;

// Структура пакета чата из вашего Задания 6
typedef struct {
    int type;
    pid_t pid;
    char nickname[MAX_NICK_LEN];
    char text[MAX_TEXT_LEN];
} ChatPacket;

// Структура для представления разобранного пакета
typedef struct {
    unsigned char src_mac[6];
    unsigned char dest_mac[6];
    struct in_addr src_ip;
    struct in_addr dest_ip;
    unsigned short src_port;
    unsigned short dest_port;
    const unsigned char *payload;
    int payload_len;
} ParsedPacket;

// Разбор сырого буфера
bool parse_udp_packet(const unsigned char *buffer, int packet_size, ParsedPacket *pkt);

// Форматированный вывод полезной нагрузки в ASCII и HEX
void print_payload(const unsigned char *payload, int len, FILE *log_file);

// Вывод информации о пакете на экран и в лог-файл (choice 1 - Чат, 2 - DNS)
void process_and_log_packet(const ParsedPacket *pkt, double elapsed, int choice, FILE *log_file);

#endif