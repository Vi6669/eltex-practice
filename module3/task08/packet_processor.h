#ifndef PACKET_PROCESSOR_H
#define PACKET_PROCESSOR_H

#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#define MAX_NICK_LEN 32
#define MAX_TEXT_LEN 512

// Типы сообщений, синхронизированные с Заданием 6
typedef enum {
    MSG_JOIN = 1,
    MSG_CHAT = 2,
    MSG_LEAVE = 3
} MsgType;

// Точная структура пакета чата из Задания 6 (для приведения типов)
typedef struct {
    int type;
    pid_t pid;
    char nickname[MAX_NICK_LEN];
    char text[MAX_TEXT_LEN];
} ChatPacket;

// Удобная структура для хранения извлеченных данных из RAW-буфера
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

// Умный разбор буфера на уровни L2 (Ethernet), L3 (IP) и L4 (UDP)
bool parse_udp_packet(const unsigned char *buffer, int packet_size, ParsedPacket *pkt);

// Печать "сырой" полезной нагрузки пакета в текстовом и шестнадцатеричном виде
void print_payload(const unsigned char *payload, int len, FILE *log_file);

// Главная функция логирования: выводит пакет на экран и пишет в capture.log
void process_and_log_packet(const ParsedPacket *pkt, double elapsed, int choice, FILE *log_file);

#endif