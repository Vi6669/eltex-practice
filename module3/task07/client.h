#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <sys/poll.h>

// Инициализация
int init_client_socket(const char *ip, int port);

// Работа с пакетами и файлами
const char *get_base_filename(const char *path);
void send_packet(int sockfd, MsgType type, const char *nickname, const char *text, const char *filename, uint32_t data_len);
void upload_file(int sockfd, const char *nickname, const char *filepath);

// Обработка событий poll
void process_user_input(int sockfd, const char *nickname);
void handle_server_message(int sockfd, FILE **rx_file);

// Главный цикл
void run_client_loop(int sockfd, const char *nickname);

#endif // CLIENT_H