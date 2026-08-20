#include "filter.h"

bool filter_chat_task6(const ParsedPacket *pkt, int chat_port) {
    if (pkt->src_port == chat_port || pkt->dest_port == chat_port) {
        return true;
    }
    return false;
}

bool filter_dns(const ParsedPacket *pkt) {
    if (pkt->src_port == 53 || pkt->dest_port == 53) {
        return true;
    }
    return false;
}