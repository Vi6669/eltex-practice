#ifndef FILTER_H
#define FILTER_H

#include <stdbool.h>
#include "packet_processor.h"

// Проверяет, совпадает ли порт пакета с портом нашего чата
bool filter_chat_task6(const ParsedPacket *pkt, int chat_port);

// Проверяет, является ли пакет DNS-запросом/ответом (порт 53)
bool filter_dns(const ParsedPacket *pkt);

#endif