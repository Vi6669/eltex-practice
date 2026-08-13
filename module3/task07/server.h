#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <sys/epoll.h>

#define MAX_CLIENTS 128
#define MAX_EVENTS 64

// Инициализация
int init_server_socket(int port);
int init_epoll(int listen_fd);

// Управление клиентами и логами
void remove_client(int client_fd);
void broadcast_packet(int sender_fd, const ChatPacket *packet);
void log_packet(const ChatPacket *packet);

// Обработка событий epoll
void handle_new_connection(int epoll_fd, int listen_fd);
void handle_client_disconnect(int epoll_fd, int client_fd);
void handle_client_data(int epoll_fd, int client_fd);

// Главный цикл
void run_server_loop(int epoll_fd, int listen_fd);

#endif // SERVER_H