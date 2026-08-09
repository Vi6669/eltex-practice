#include "chat.h"

volatile sig_atomic_t running = 1;

// Настройка UDP сокета с поддержкой Broadcast и ReusePort
int setup_broadcast_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка при создании сокета");
        return -1;
    }

    int opt = 1;
    // Разрешаем повторное использование адреса
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Ошибка setsockopt SO_REUSEADDR");
    }

    // Разрешаем повторное использование порта (критично для запуска копий программы на одной машине)
#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("Ошибка setsockopt SO_REUSEPORT");
    }
#endif

    // Включаем поддержку широковещательной рассылки
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("Ошибка setsockopt SO_BROADCAST");
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Слушаем на всех интерфейсах

    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("Ошибка при вызове bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Отправка сетевого пакета
void send_packet(int sockfd, const struct sockaddr_in *dest_addr, MsgType type, const char *nickname, const char *text) {
    ChatPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = type;
    packet.pid = getpid();
    strncpy(packet.nickname, nickname, MAX_NICK_LEN - 1);
    
    if (text) {
        strncpy(packet.text, text, MAX_TEXT_LEN - 1);
    }

    if (sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("Ошибка при отправке пакета (sendto)");
    }
}

// Прием и обработка входящего сетевого пакета
void handle_incoming_packet(int sockfd) {
    ChatPacket packet;
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr*)&src_addr, &addr_len);
    if (n < 0) {
        if (errno != EINTR) {
            perror("Ошибка при чтении из сокета (recvfrom)");
        }
        return;
    }

    // Игнорируем эхо-пакеты, пришедшие от самого себя
    if (packet.pid == getpid()) {
        return;
    }

    // Обеспечиваем корректное завершение строк
    packet.nickname[MAX_NICK_LEN - 1] = '\0';
    packet.text[MAX_TEXT_LEN - 1] = '\0';

    switch (packet.type) {
        case MSG_JOIN:
            printf("\r\033[32m[+] В сети появился новый участник: %s (PID: %d)\033[0m\n", packet.nickname, packet.pid);
            break;
        case MSG_CHAT:
            printf("\r\033[36m[%s]:\033[0m %s\n", packet.nickname, packet.text);
            break;
        case MSG_LEAVE:
            printf("\r\033[31m[-] %s (PID: %d) вышел из сети\033[0m\n", packet.nickname, packet.pid);
            break;
        default:
            break;
    }

    // Возвращаем приглашение к вводу
    printf("\r> ");
    fflush(stdout);
}

// Обработка ввода пользователя с клавиатуры
void handle_user_input(int sockfd, const struct sockaddr_in *dest_addr, const char *nickname) {
    char buffer[MAX_TEXT_LEN];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return;
    }

    // Удаляем символ переноса строки на конце
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    // Игнорируем пустые сообщения
    if (strlen(buffer) == 0) {
        printf("> ");
        fflush(stdout);
        return;
    }

    // Команда на выход из чата
    if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
        running = 0;
        return;
    }

    // Выводим собственное сообщение на экран
    printf("\033[34m[Вы]:\033[0m %s\n", buffer);
    printf("> ");
    fflush(stdout);

    // Рассылаем сообщение остальным
    send_packet(sockfd, dest_addr, MSG_CHAT, nickname, buffer);
}

// Сигнальный обработчик Ctrl+C
void sigint_handler(int sig) {
    (void)sig; 
    running = 0; // Корректно завершаем цикл ожидания
}