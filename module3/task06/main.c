#include "chat.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Использование: %s <broadcast_ip> <port> <nickname>\n", argv[0]);
        fprintf(stderr, "Пример для дома: %s 255.255.255.255 55555 Ivan\n", argv[0]);
        return 1;
    }

    const char *broadcast_ip = argv[1];
    int port = atoi(argv[2]);
    const char *nickname = argv[3];

    // Настраиваем перехват сигнала Ctrl+C для отправки сообщения о выходе
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    // Настраиваем адрес назначения для бродкаста
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, broadcast_ip, &dest_addr.sin_addr) <= 0) {
        fprintf(stderr, "Ошибка: некорректный широковещательный IP-адрес: %s\n", broadcast_ip);
        return 1;
    }

    // Инициализируем сокет
    int sockfd = setup_broadcast_socket(port);
    if (sockfd < 0) {
        return 1;
    }

    printf("=== Групповой UDP Чат запущен ===\n");
    printf("Никнейм: %s | Порт: %d | Broadcast IP: %s\n", nickname, port, broadcast_ip);
    printf("Для выхода введите 'exit' или нажмите Ctrl+C\n");
    printf("==================================\n\n");

    // Отправляем первое сообщение: "Я в сети"
    send_packet(sockfd, &dest_addr, MSG_JOIN, nickname, NULL);

    printf("> ");
    fflush(stdout);

    int max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
    fd_set read_fds;

    // Главный цикл мультиплексирования
    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            if (errno == EINTR) {
                // Прервано сигналом Ctrl+C, выходим из цикла
                break;
            }
            perror("Ошибка в системном вызове select");
            break;
        }

        // Пришел пакет из сети
        if (FD_ISSET(sockfd, &read_fds)) {
            handle_incoming_packet(sockfd);
        }

        // Пользователь ввел текст в консоли
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            handle_user_input(sockfd, &dest_addr, nickname);
        }
    }

    // Завершение работы программы
    printf("\n\nВыход из чата...\n");
    send_packet(sockfd, &dest_addr, MSG_LEAVE, nickname, NULL);
    close(sockfd);

    return 0;
}