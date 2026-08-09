#ifndef CHAT_H
#define CHAT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAX_NICK_LEN 32
#define MAX_TEXT_LEN 512

// Типы сообщений
typedef enum {
    MSG_JOIN = 1,  // Подключение к сети
    MSG_CHAT = 2,  // Обычное сообщение
    MSG_LEAVE = 3  // Отключение от сети
} MsgType;

// Структура сетевого пакета
typedef struct {
    int type;                       // Тип сообщения (MsgType)
    pid_t pid;                      // PID отправителя (чтобы отсекать собственные эхо-пакеты)
    char nickname[MAX_NICK_LEN];    // Никнейм отправителя
    char text[MAX_TEXT_LEN];        // Текст сообщения
} ChatPacket;

// Флаг работы главного цикла
extern volatile sig_atomic_t running;

// Прототипы функций
int setup_broadcast_socket(int port);
void send_packet(int sockfd, const struct sockaddr_in *dest_addr, MsgType type, const char *nickname, const char *text);
void handle_incoming_packet(int sockfd);
void handle_user_input(int sockfd, const struct sockaddr_in *dest_addr, const char *nickname);
void sigint_handler(int sig);

#endif // CHAT_H