#ifndef SNIFFER_H
#define SNIFFER_H

#include <time.h>
#include <stdbool.h>

// Инициализация низкоуровневого RAW-сокета
int init_raw_socket(const char *device);

// Включение/отключение беспорядочного режима (Promiscuous mode)
int set_promisc_mode(int sock, const char *device, bool enable);

// Получение времени, прошедшего с момента запуска утилиты
double get_elapsed_time(struct timespec start_time);

// Преобразование байтового массива MAC-адреса в строку
void format_mac(const unsigned char *mac, char *out_str);

#endif