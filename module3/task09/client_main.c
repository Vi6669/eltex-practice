#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "client_logic.h"

// Сигнальный флаг для безопасного выхода из бесконечного цикла
volatile sig_atomic_t keep_running = 1;

// Обработчик системного сигнала SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <local_ip> <local_port> <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    const char *local_ip = argv[1];
    uint16_t client_port = (uint16_t)atoi(argv[2]);
    const char *server_ip = argv[3];
    uint16_t server_port = (uint16_t)atoi(argv[4]);

    uint32_t local_ip_bin = inet_addr(local_ip);
    uint32_t server_ip_bin = inet_addr(server_ip);

    // Регистрация обработчика сигнала завершения работы
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed (are you root? sudo is required for RAW sockets)");
        exit(1);
    }

    // Резервирование локального порта для защиты от ложных ICMP-пакетов операционной системы
    int dummy_fd = reserve_local_port(local_ip_bin, client_port);

    // Установка тайм-аута на сокет в 1 секунду
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("[CLIENT] Started RAW UDP client on %s:%d sending to %s:%d\n", local_ip, client_port, server_ip, server_port);
    printf("[CLIENT] Type message and press Enter (Ctrl+C to exit cleanly):\n");

    char input_buf[1024];
    int print_prompt = 1; // Флаг предотвращения бесконечных стрелочек при таймаутах

    while (keep_running) {
        if (print_prompt) {
            printf("> ");
            fflush(stdout);
            print_prompt = 0;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 }; // 100ms

        // Используем select для неблокирующего сканирования stdin
        int sel = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
        if (sel < 0) {
            break; // Выход при получении сигнала прерывания
        }
        if (sel == 0) {
            continue; // Таймаут select, возвращаемся к началу цикла без вывода стрелочки
        }

        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
            break;
        }

        input_buf[strcspn(input_buf, "\r\n")] = '\0';
        if (strlen(input_buf) == 0) {
            print_prompt = 1;
            continue;
        }

        // Отправка сырого сообщения
        send_raw_message(sockfd, local_ip_bin, server_ip_bin, client_port, server_port, input_buf);

        // Ожидание ответа
        int received = wait_for_server_reply(sockfd, client_port, server_port);
        if (!received) {
            printf("[CLIENT] No reply from server (timeout).\n");
        }

        print_prompt = 1;
    }

    // Отправка сигнала закрытия серверу перед отключением
    send_clean_shutdown(sockfd, local_ip_bin, server_ip_bin, client_port, server_port);

    if (dummy_fd >= 0) close(dummy_fd);
    close(sockfd);
    return 0;
}