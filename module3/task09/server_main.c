#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "server_logic.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(1);
    }

    uint16_t server_port = (uint16_t)atoi(argv[1]);

    // Создаем RAW сокет с типом взаимодействия SOCK_RAW и протоколом IPPROTO_UDP
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed (are you root? sudo is required for RAW sockets)");
        exit(1);
    }

    init_client_db();
    printf("[SERVER] Started RAW socket receiver on port %d...\n", server_port);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in saddr;
    socklen_t saddr_len = sizeof(saddr);

    // Главный цикл приема сырых пакетов
    while (1) {
        int packet_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&saddr, &saddr_len);
        if (packet_len < 0) {
            perror("Recvfrom failed");
            continue;
        }

        // Передаем пакет на разбор и обработку в слой бизнес-логики
        process_incoming_packet(sockfd, buffer, packet_len, server_port);
    }

    close(sockfd);
    return 0;
}