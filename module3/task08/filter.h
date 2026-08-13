#ifndef FILTER_H
#define FILTER_H

#include <stdbool.h>
#include "packet_processor.h"

// Фильтр для сообщений группового чата (Задание 6) с динамическим портом
bool filter_chat_task6(const ParsedPacket *pkt, int chat_port);

// Фильтр для DNS-сообщений
bool filter_dns(const ParsedPacket *pkt);

#endif