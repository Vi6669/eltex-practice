#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

#define MAX_NICK_LEN 32
#define MAX_CHUNK_SIZE 1024
#define MAX_FILENAME 256

// Типы прикладных пакетов
typedef enum {
    MSG_JOIN = 1,        // Подключение клиента
    MSG_CHAT = 2,        // Текстовое сообщение
    MSG_LEAVE = 3,       // Выход клиента
    MSG_FILE_START = 4,  // Инициализация отправки файла
    MSG_FILE_DATA = 5,   // Порция данных файла (chunk)
    MSG_FILE_END = 6     // Окончание отправки файла
} MsgType;

// Единая прикладная структура пакета
typedef struct {
    int32_t type;                       // Тип сообщения (MsgType)
    uint32_t data_len;                  // Размер актуальных данных в payload
    char nickname[MAX_NICK_LEN];        // Никнейм отправителя
    char filename[MAX_FILENAME];        // Имя передаваемого файла
    char payload[MAX_CHUNK_SIZE];       // Текст сообщения или бинарные данные
} ChatPacket;

// TCP Помощники для чтения/записи фиксированных структур
ssize_t read_all(int fd, void *buf, size_t size);
ssize_t write_all(int fd, const void *buf, size_t size);

#endif // COMMON_H