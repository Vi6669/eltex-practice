#ifndef SNIFFER_H
#define SNIFFER_H

#include <time.h>
#include <stdbool.h>

// Инициализация низкоуровневого RAW-сокета на указанном интерфейсе
int init_raw_socket(const char *device);

// Включение или отключение "беспорядочного" режима (Promiscuous mode).
// Позволяет сетевой карте принимать пакеты, адресованные другим узлам.
int set_promisc_mode(int sock, const char *device, bool enable);

// Вычисление времени в секундах, прошедшего с момента старта (start_time)
double get_elapsed_time(struct timespec start_time);

// Преобразование массива из 6 байт MAC-адреса в читаемую строку (xx:xx:xx:xx:xx:xx)
void format_mac(const unsigned char *mac, char *out_str);

#endif